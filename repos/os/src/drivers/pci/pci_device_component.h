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

#include <pci_device/pci_device.h>
#include <util/list.h>
#include <base/rpc_server.h>
#include <base/printf.h>

#include <io_mem_session/io_mem_session.h>
#include <irq_session/connection.h>

#include "pci_device_config.h"

namespace Pci {

	class Device_component : public Genode::Rpc_object<Device>,
	                         public Genode::List<Device_component>::Element
	{
		private:

			Device_config              _device_config;
			Genode::addr_t             _config_space;
			Genode::Io_mem_connection *_io_mem;
			Config_access              _config_access;
			Genode::Irq_connection     _irq;

			enum { PCI_IRQ = 0x3c };

		public:

			/**
			 * Constructor
			 */
			Device_component(Device_config device_config, Genode::addr_t addr)
			:
				_device_config(device_config), _config_space(addr),
				_io_mem(0),
				_irq(_device_config.read(&_config_access, PCI_IRQ,
				                         Pci::Device::ACCESS_8BIT))
			{ }

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

			unsigned short vendor_id() {
				return _device_config.vendor_id(); }

			unsigned short device_id() {
				return _device_config.device_id(); }

			unsigned class_code() {
				return _device_config.class_code(); }

			Resource resource(int resource_id)
			{
				/* return invalid resource if device is invalid */
				if (!_device_config.valid())
					Resource(0, 0);

				return _device_config.resource(resource_id);
			}

			unsigned config_read(unsigned char address, Access_size size)
			{
				Config_access config_access;

				return _device_config.read(&config_access, address, size);
			}

			void config_write(unsigned char address, unsigned value, Access_size size)
			{
				Config_access config_access;

				/*
				 * XXX implement policy to prevent write access to base-address registers
				 */

				_device_config.write(&config_access, address, value, size);
			}

			Genode::Irq_session_capability irq(Genode::uint8_t id) override
			{
				if (id != 0)
					return Genode::Irq_session_capability();
				return _irq_session.cap();
			}
	};
}

#endif /* _PCI_DEVICE_COMPONENT_H_ */
