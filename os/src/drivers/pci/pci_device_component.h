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

#include "pci_device_config.h"

namespace Pci {

	class Device_component : public Genode::Rpc_object<Device>,
	                         public Genode::List<Device_component>::Element
	{
		private:

			Device_config _device_config;

		public:

			/**
			 * Constructor
			 */
			Device_component(Device_config device_config):
				_device_config(device_config) { }

			Device_config config() { return _device_config; }


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
	};
}

#endif /* _PCI_DEVICE_COMPONENT_H_ */
