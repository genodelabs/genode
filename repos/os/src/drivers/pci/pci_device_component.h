/*
 * \brief  PCI-device component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PCI_DEVICE_COMPONENT_H_
#define _PCI_DEVICE_COMPONENT_H_

/* base */
#include <base/rpc_server.h>
#include <io_mem_session/connection.h>
#include <util/list.h>
#include <util/mmio.h>

/* os */
#include <pci_device/pci_device.h>

/* local */
#include "pci_device_config.h"
#include "irq.h"

namespace Pci { class Device_component; class Session_component; }

class Pci::Device_component : public Genode::Rpc_object<Pci::Device>,
                              public Genode::List<Device_component>::Element
{
	private:

		Device_config              _device_config;
		Genode::addr_t             _config_space;
		Genode::Io_mem_connection *_io_mem;
		Config_access              _config_access;
		Genode::Rpc_entrypoint    *_ep;
		Pci::Session_component    *_session;
		unsigned short             _irq_line;
		Irq_session_component      _irq_session;
		bool                       _rewrite_irq_line;

		enum {
			IO_BLOCK_SIZE = sizeof(Genode::Io_port_connection) *
			                Device::NUM_RESOURCES + 32 + 8 * sizeof(void *),
			PCI_IRQ_LINE  = 0x3c
		};

		Genode::Tslab<Genode::Io_port_connection, IO_BLOCK_SIZE> _slab_ioport;
		Genode::Slab_block _slab_ioport_block;
		char _slab_ioport_block_data[IO_BLOCK_SIZE];

		Genode::Tslab<Genode::Io_mem_connection, IO_BLOCK_SIZE> _slab_iomem;
		Genode::Slab_block _slab_iomem_block;
		char _slab_iomem_block_data[IO_BLOCK_SIZE];

		Genode::Io_port_connection *_io_port_conn [Device::NUM_RESOURCES];
		Genode::Io_mem_connection  *_io_mem_conn  [Device::NUM_RESOURCES];

		struct Status : Genode::Register<8> {
			struct Capabilities : Bitfield<4,1> { };

			inline static access_t read(Genode::uint8_t t) { return t; };
		};


		/**
		 * Read out msi capabilities of the device.
		 */
		Genode::uint16_t _msi_cap()
		{
			enum { PCI_STATUS = 0x6, PCI_CAP_OFFSET = 0x34, CAP_MSI = 0x5 };

			Status::access_t status = Status::read(_device_config.read(&_config_access,
			                                       PCI_STATUS,
			                                       Pci::Device::ACCESS_16BIT));
			if (!Status::Capabilities::get(status))
				return 0;

			Genode::uint8_t cap = _device_config.read(&_config_access,
			                                          PCI_CAP_OFFSET,
			                                          Pci::Device::ACCESS_8BIT);

			for (Genode::uint16_t val = 0; cap; cap = val >> 8) {
				val = _device_config.read(&_config_access, cap,
				                          Pci::Device::ACCESS_16BIT);
				if ((val & 0xff) != CAP_MSI)
					continue;

				return cap;
			}

			return 0;
		}


		/**
		 * Disable MSI if already enabled.
		 */
		unsigned _disable_msi(unsigned irq) {

			using Genode::uint16_t;

			uint16_t cap = _msi_cap();
			if (!cap)
				return irq;

			uint16_t msi = _device_config.read(&_config_access, cap + 2,
			                                   Pci::Device::ACCESS_16BIT);

			enum { MSI_ENABLED = 0x1 };

			if (msi & MSI_ENABLED)
				/* disable MSI */
				_device_config.write(&_config_access, cap + 2,
				                     msi ^ MSI_ENABLED,
				                     Pci::Device::ACCESS_8BIT);

			return irq;
		}

	public:

		/**
		 * Constructor
		 */
		Device_component(Device_config device_config, Genode::addr_t addr,
		                 Genode::Rpc_entrypoint *ep,
		                 Pci::Session_component * session,
		                 bool rewrite_irq_line)
		:
			_device_config(device_config), _config_space(addr),
			_io_mem(0), _ep(ep), _session(session),
			_irq_line(_device_config.read(&_config_access, PCI_IRQ_LINE,
			                              Pci::Device::ACCESS_8BIT)),
			_irq_session(_disable_msi(_irq_line), _msi_cap() ? _config_space : 0),
			_rewrite_irq_line(rewrite_irq_line),
			_slab_ioport(0, &_slab_ioport_block),
			_slab_iomem(0, &_slab_iomem_block)
		{
			_ep->manage(&_irq_session);

			for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
				_io_port_conn[i] = nullptr;
				_io_mem_conn[i] = nullptr;
			}

			if (_slab_ioport.num_elem() != Device::NUM_RESOURCES)
				PERR("incorrect amount of space for io port resources");
			if (_slab_iomem.num_elem() != Device::NUM_RESOURCES)
				PERR("incorrect amount of space for io mem resources");

			if (!_irq_session.msi())
				return;

			Genode::addr_t msi_address = _irq_session.msi_address();
			Genode::uint32_t msi_value = _irq_session.msi_data();
			Genode::uint16_t msi_cap   = _msi_cap();

			enum { CAP_MSI_64 = 0x80, MSI_ENABLED = 0x1 };
			Genode::uint16_t msi = _device_config.read(&_config_access,
			                                           msi_cap + 2,
			                                           Pci::Device::ACCESS_16BIT);

			_device_config.write(&_config_access, msi_cap + 0x4, msi_address,
			                     Pci::Device::ACCESS_32BIT);

			if (msi & CAP_MSI_64) {
				Genode::uint32_t upper_address = sizeof(msi_address) > 4 ?
				                                 msi_address >> 32 : 0UL;

				_device_config.write(&_config_access, msi_cap + 0x8,
				                     upper_address,
				                     Pci::Device::ACCESS_32BIT);
				_device_config.write(&_config_access, msi_cap + 0xc,
				                     msi_value,
				                     Pci::Device::ACCESS_16BIT);
			}
			else
				_device_config.write(&_config_access, msi_cap + 0x8, msi_value,
				                     Pci::Device::ACCESS_16BIT);

			/* enable MSI */
			_device_config.write(&_config_access, msi_cap + 2,
			                     msi ^ MSI_ENABLED,
			                     Pci::Device::ACCESS_8BIT);
		}

		/**
		 * Constructor for non PCI devices
		 */
		Device_component(Genode::Rpc_entrypoint * ep,
		                 Pci::Session_component * session, unsigned irq)
		:
			_config_space(~0UL), _io_mem(0), _ep(ep), _session(session),
			_irq_line(irq),
			_irq_session(_irq_line, 0),
			_slab_ioport(0, &_slab_ioport_block),
			_slab_iomem(0, &_slab_iomem_block)
		{
			_ep->manage(&_irq_session);

			for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
				_io_port_conn[i] = nullptr;
				_io_mem_conn[i] = nullptr;
			}
		}

		/**
		 * De-constructor
		 */
		~Device_component()
		{
			_ep->dissolve(&_irq_session);

			for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
				if (_io_port_conn[i])
					Genode::destroy(_slab_ioport, _io_port_conn[i]);
				if (_io_mem_conn[i])
					Genode::destroy(_slab_iomem, _io_mem_conn[i]);
			}
		}

		/****************************************
		 ** Methods used solely by pci session **
		 ****************************************/

		Device_config config() { return _device_config; }

		Genode::addr_t config_space() { return _config_space; }

		void set_config_space(Genode::Io_mem_connection * io_mem) {
			_io_mem = io_mem; }

		Genode::Io_mem_connection * get_config_space() { return _io_mem; }

		/**************************
		 ** PCI-device interface **
		 **************************/

		void bus_address(unsigned char *bus, unsigned char *dev,
		                 unsigned char *fn)
		{
			*bus = _device_config.bus_number();
			*dev = _device_config.device_number();
			*fn  = _device_config.function_number();
		}

		unsigned short vendor_id() { return _device_config.vendor_id(); }

		unsigned short device_id() { return _device_config.device_id(); }

		unsigned class_code() { return _device_config.class_code(); }

		Resource resource(int resource_id)
		{
			/* return invalid resource if device is invalid */
			if (!_device_config.valid())
				return Resource(0, 0);

			return _device_config.resource(resource_id);
		}

		unsigned config_read(unsigned char address, Access_size size)
		{
			return _device_config.read(&_config_access, address, size);
		}

		void config_write(unsigned char address, unsigned value, Access_size size)
		{
			/* white list of ports which we permit to write */
			switch(address) {
				case 0x4: /* COMMAND register - first byte */
					if (size == Access_size::ACCESS_16BIT)
						break;
				case 0x5: /* COMMAND register - second byte */
				case 0xd: /* Latency timer */
					if (size == Access_size::ACCESS_8BIT)
						break;
				case PCI_IRQ_LINE:
					/* permitted up to now solely for acpi driver */
					if (address == PCI_IRQ_LINE && _rewrite_irq_line &&
					    size == Access_size::ACCESS_8BIT)
						break;
				default:
					PWRN("%x:%x:%x write access to address=%x value=0x%x "
					     " size=0x%x got dropped", _device_config.bus_number(),
						_device_config.device_number(),
						_device_config.function_number(),
						address, value, size);
					return;
			}

			_device_config.write(&_config_access, address, value, size);
		}

		Genode::Irq_session_capability irq(Genode::uint8_t id) override
		{
			if (id != 0)
				return Genode::Irq_session_capability();
			return _irq_session.cap();
		}

		Genode::Io_port_session_capability io_port(Genode::uint8_t) override;

		Genode::Io_mem_session_capability io_mem(Genode::uint8_t) override;
};

#endif /* _PCI_DEVICE_COMPONENT_H_ */
