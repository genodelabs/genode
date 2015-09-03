/*
 * \brief  platform device component
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

/* base */
#include <base/rpc_server.h>
#include <io_mem_session/connection.h>
#include <util/list.h>
#include <util/mmio.h>

/* os */
#include <platform_device/platform_device.h>

/* local */
#include "pci_device_config.h"
#include "irq.h"

namespace Platform { class Device_component; class Session_component; }

class Platform::Device_component : public Genode::Rpc_object<Platform::Device>,
                                   public Genode::List<Device_component>::Element
{
	private:

		Device_config                      _device_config;
		Genode::addr_t                     _config_space;
		Genode::Io_mem_session_capability  _io_mem_config_extended;
		Config_access                      _config_access;
		Genode::Rpc_entrypoint            *_ep;
		Platform::Session_component       *_session;
		unsigned short                     _irq_line;
		Irq_session_component              _irq_session;

		enum {
			IO_BLOCK_SIZE = sizeof(Genode::Io_port_connection) *
			                Device::NUM_RESOURCES + 32 + 8 * sizeof(void *),
			PCI_CMD_REG   = 0x4,
			PCI_CMD_DMA   = 0x4,
			PCI_IRQ_LINE  = 0x3c,
			PCI_IRQ_PIN   = 0x3d,

			CAP_MSI_64    = 0x80,
			MSI_ENABLED   = 0x1
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
			                                       Platform::Device::ACCESS_16BIT));
			if (!Status::Capabilities::get(status))
				return 0;

			Genode::uint8_t cap = _device_config.read(&_config_access,
			                                          PCI_CAP_OFFSET,
			                                          Platform::Device::ACCESS_8BIT);

			for (Genode::uint16_t val = 0; cap; cap = val >> 8) {
				val = _device_config.read(&_config_access, cap,
				                          Platform::Device::ACCESS_16BIT);
				if ((val & 0xff) != CAP_MSI)
					continue;

				return cap;
			}

			return 0;
		}


		/**
		 * Disable MSI if already enabled.
		 */
		unsigned _configure_irq(unsigned irq)
		{
			using Genode::uint16_t;
			using Genode::uint8_t;

			uint8_t pin = _device_config.read(&_config_access, PCI_IRQ_PIN,
			                                  Platform::Device::ACCESS_8BIT);
			if (!pin)
				return Irq_session_component::INVALID_IRQ;

			/* lookup rewrite information as provided by acpi table */
			uint16_t irq_r = Irq_routing::rewrite(_device_config.bus_number(),
			                                      _device_config.device_number(),
			                                      _device_config.function_number(),
			                                      pin);
			if (irq_r) {
				PINF("%x:%x.%x rewriting IRQ: %u -> %u",
				     _device_config.bus_number(),
				     _device_config.device_number(),
				     _device_config.function_number(), irq, irq_r);

				if (_irq_line != irq_r)
					_device_config.write(&_config_access, PCI_IRQ_LINE, irq_r,
					                     Platform::Device::ACCESS_8BIT);

				_irq_line = irq = irq_r;
			}

			uint16_t cap = _msi_cap();
			if (!cap)
				return irq;

			uint16_t msi = _device_config.read(&_config_access, cap + 2,
			                                   Platform::Device::ACCESS_16BIT);

			if (msi & MSI_ENABLED)
				/* disable MSI */
				_device_config.write(&_config_access, cap + 2,
				                     msi ^ MSI_ENABLED,
				                     Platform::Device::ACCESS_8BIT);

			return irq;
		}


		/**
		 * Disable bus master dma if already enabled.
		 */
		void _disable_bus_master_dma() {

			/*
			 * Disabling a bridge may make the devices behind non-functional,
			 * as we have no driver which will switch it on again
			 */
			if (_device_config.is_pci_bridge())
				return;

			unsigned cmd = _device_config.read(&_config_access, PCI_CMD_REG,
			                                   Platform::Device::ACCESS_16BIT);
			if (cmd & PCI_CMD_DMA)
				_device_config.write(&_config_access, PCI_CMD_REG,
				                     cmd ^ PCI_CMD_DMA,
				                     Platform::Device::ACCESS_16BIT);
		}


	public:

		/**
		 * Constructor
		 */
		Device_component(Device_config device_config, Genode::addr_t addr,
		                 Genode::Rpc_entrypoint *ep,
		                 Platform::Session_component * session,
		                 bool use_msi)
		:
			_device_config(device_config), _config_space(addr),
			_ep(ep), _session(session),
			_irq_line(_device_config.read(&_config_access, PCI_IRQ_LINE,
			                              Platform::Device::ACCESS_8BIT)),
			_irq_session(_configure_irq(_irq_line), (!use_msi || !_msi_cap()) ? ~0UL : _config_space),
			_slab_ioport(0, &_slab_ioport_block),
			_slab_iomem(0, &_slab_iomem_block)
		{
			if (_config_space != ~0UL) {
				try {
					Genode::Io_mem_connection conn(_config_space, 0x1000);
					conn.on_destruction(Genode::Io_mem_connection::KEEP_OPEN);
					_io_mem_config_extended = conn;
				} catch (Genode::Parent::Service_denied) { }
			}

			_ep->manage(&_irq_session);

			for (unsigned i = 0; i < Device::NUM_RESOURCES; i++) {
				_io_port_conn[i] = nullptr;
				_io_mem_conn[i] = nullptr;
			}

			if (_slab_ioport.num_elem() != Device::NUM_RESOURCES)
				PERR("incorrect amount of space for io port resources");
			if (_slab_iomem.num_elem() != Device::NUM_RESOURCES)
				PERR("incorrect amount of space for io mem resources");

			_disable_bus_master_dma();

			if (!_irq_session.msi())
				return;

			Genode::addr_t msi_address = _irq_session.msi_address();
			Genode::uint32_t msi_value = _irq_session.msi_data();
			Genode::uint16_t msi_cap   = _msi_cap();

			Genode::uint16_t msi = _device_config.read(&_config_access,
			                                           msi_cap + 2,
			                                           Platform::Device::ACCESS_16BIT);

			_device_config.write(&_config_access, msi_cap + 0x4, msi_address,
			                     Platform::Device::ACCESS_32BIT);

			if (msi & CAP_MSI_64) {
				Genode::uint32_t upper_address = sizeof(msi_address) > 4
				                               ? (Genode::uint64_t)msi_address >> 32
				                               : 0UL;

				_device_config.write(&_config_access, msi_cap + 0x8,
				                     upper_address,
				                     Platform::Device::ACCESS_32BIT);
				_device_config.write(&_config_access, msi_cap + 0xc,
				                     msi_value,
				                     Platform::Device::ACCESS_16BIT);
			}
			else
				_device_config.write(&_config_access, msi_cap + 0x8, msi_value,
				                     Platform::Device::ACCESS_16BIT);

			/* enable MSI */
			_device_config.write(&_config_access, msi_cap + 2,
			                     msi ^ MSI_ENABLED,
			                     Platform::Device::ACCESS_8BIT);
		}

		/**
		 * Constructor for non PCI devices
		 */
		Device_component(Genode::Rpc_entrypoint * ep,
		                 Platform::Session_component * session, unsigned irq)
		:
			_config_space(~0UL),
			_ep(ep), _session(session),
			_irq_line(irq),
			_irq_session(_irq_line, _config_space),
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

			if (_io_mem_config_extended.valid())
				Genode::env()->parent()->close(_io_mem_config_extended);

			if (_device_config.valid())
				_disable_bus_master_dma();
		}

		/****************************************
		 ** Methods used solely by pci session **
		 ****************************************/

		Device_config config() { return _device_config; }

		Genode::Io_mem_dataspace_capability get_config_space()
		{
			if (!_io_mem_config_extended.valid())
				return Genode::Io_mem_dataspace_capability();

			Genode::Io_mem_session_client client(_io_mem_config_extended);
			return client.dataspace();
		}

		/**************************
		 ** PCI-device interface **
		 **************************/

		void bus_address(unsigned char *bus, unsigned char *dev,
		                 unsigned char *fn) override
		{
			*bus = _device_config.bus_number();
			*dev = _device_config.device_number();
			*fn  = _device_config.function_number();
		}

		unsigned short vendor_id() override { return _device_config.vendor_id(); }

		unsigned short device_id() override { return _device_config.device_id(); }

		unsigned class_code() override { return _device_config.class_code(); }

		Resource resource(int resource_id) override
		{
			/* return invalid resource if device is invalid */
			if (!_device_config.valid())
				return Resource(0, 0);

			return _device_config.resource(resource_id);
		}

		unsigned config_read(unsigned char address, Access_size size) override
		{
			return _device_config.read(&_config_access, address, size,
			                           _device_config.DONT_TRACK_ACCESS);
		}

		void config_write(unsigned char address, unsigned value,
		                  Access_size size) override;

		Genode::Irq_session_capability irq(Genode::uint8_t id) override
		{
			if (id != 0)
				return Genode::Irq_session_capability();

			if (!_device_config.valid())
				return _irq_session.cap();

			bool msi_64 = false;
			Genode::uint16_t msi_cap   = _msi_cap();
			if (msi_cap) {
				Genode::uint16_t msi = _device_config.read(&_config_access,
				                                           msi_cap + 2,
				                                           Platform::Device::ACCESS_16BIT);
				msi_64 = msi & CAP_MSI_64;
			}

			if (_irq_session.msi())
				PINF("%x:%x.%x uses MSI %s, vector 0x%lx, address 0x%lx",
				     _device_config.bus_number(),
				     _device_config.device_number(),
				     _device_config.function_number(),
				     msi_64 ? "64bit" : "32bit",
				     _irq_session.msi_data(), _irq_session.msi_address());
			else
				PINF("%x:%x.%x uses IRQ, vector 0x%x%s",
				     _device_config.bus_number(),
				     _device_config.device_number(),
				     _device_config.function_number(), _irq_line,
				     msi_cap ? (msi_64 ? ", MSI 64bit capable" :
				                         ", MSI 32bit capable") : "");

			return _irq_session.cap();
		}

		Genode::Io_port_session_capability io_port(Genode::uint8_t) override;

		Genode::Io_mem_session_capability io_mem(Genode::uint8_t) override;
};
