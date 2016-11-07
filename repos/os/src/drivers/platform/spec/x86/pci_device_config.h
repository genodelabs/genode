/*
 * \brief  PCI device configuration
 * \author Norman Feske
 * \date   2008-01-29
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _X86__PCI_DEVICE_CONFIG_H_
#define _X86__PCI_DEVICE_CONFIG_H_

#include <base/output.h>
#include <platform_device/platform_device.h>
#include "pci_config_access.h"

namespace Platform {

	class Device_config
	{
		private:

			int _bus, _device, _function;     /* location at PCI bus */

			/*
			 * Information provided by the PCI config space
			 */
			unsigned _vendor_id, _device_id;
			unsigned _class_code;
			unsigned _header_type;

			/*
			 * Header type definitions
			 */
			enum {
				HEADER_FUNCTION   = 0,
				HEADER_PCI_TO_PCI = 1,
				HEADER_CARD_BUS   = 2
			};

			Device::Resource _resource[Device::NUM_RESOURCES];

			bool _resource_id_is_valid(int resource_id)
			{
				/*
				 * The maximum number of PCI resources depends on the
				 * header type of the device.
				 */
				int max_num = _header_type == HEADER_FUNCTION   ? Device::NUM_RESOURCES
				            : _header_type == HEADER_PCI_TO_PCI ? 2
				            : 0;

				return resource_id >= 0 && resource_id < max_num;
			}

			enum { INVALID_VENDOR = 0xffffU };

		public:

			enum { MAX_BUSES = 256, MAX_DEVICES = 32, MAX_FUNCTIONS = 8 };

			/**
			 * Constructor
			 */
			Device_config() : _vendor_id(INVALID_VENDOR) { }

			Device_config(int bus, int device, int function,
			              Config_access *pci_config):
				_bus(bus), _device(device), _function(function)
			{
				_vendor_id = pci_config->read(bus, device, function, 0, Device::ACCESS_16BIT);

				/* break here if device is invalid */
				if (_vendor_id == INVALID_VENDOR)
					return;

				_device_id    = pci_config->read(bus, device, function, 2, Device::ACCESS_16BIT);
				_class_code   = pci_config->read(bus, device, function, 8, Device::ACCESS_32BIT) >> 8;
				_class_code  &= 0xffffff;
				_header_type  = pci_config->read(bus, device, function, 0xe, Device::ACCESS_8BIT);
				_header_type &= 0x7f;

				/*
				 * We prevent scanning function 1-7 of non-multi-function
				 * devices by checking bit 7 (mf bit) of function 0 of the
				 * device. Note, the mf bit of function 1-7 is not significant
				 * and may be set or unset.
				 */
				if (function != 0
				 && !(pci_config->read(bus, device, 0, 0xe, Device::ACCESS_8BIT) & 0x80)) {
					_vendor_id = INVALID_VENDOR;
					return;
				}

				for (int i = 0; _resource_id_is_valid(i); i++) {

					/* index of base-address register in configuration space */
					unsigned bar_idx = 0x10 + 4 * i;

					/* read original base-address register value */
					unsigned orig_bar = pci_config->read(bus, device, function, bar_idx, Device::ACCESS_32BIT);

					/* check for invalid resource */
					if (orig_bar == (unsigned)~0) {
						_resource[i] = Device::Resource(0, 0);
						continue;
					}

					/*
					 * Determine resource size by writing a magic value (all bits set)
					 * to the base-address register. In response, the device clears a number
					 * of lowest-significant bits corresponding to the resource size.
					 * Finally, we write back the original value as assigned by the BIOS.
					 */
					pci_config->write(bus, device, function, bar_idx, ~0, Device::ACCESS_32BIT);
					unsigned bar = pci_config->read(bus, device, function, bar_idx, Device::ACCESS_32BIT);
					pci_config->write(bus, device, function, bar_idx, orig_bar, Device::ACCESS_32BIT);

					/*
					 * Scan base-address-register value for the lowest set bit but
					 * ignore the lower bits that are used to describe the resource type.
					 * I/O resources use the lowest 3 bits, memory resources use the
					 * lowest four bits for the resource description.
					 */
					unsigned start = (bar & 1) ? 3 : 4;
					unsigned size  = 1 << start;
					for (unsigned bit = start; bit < 32; bit++, size += size)

						/* stop at the lowest-significant set bit */
						if (bar & (1 << bit))
							break;

					_resource[i] = Device::Resource(orig_bar, size);
				}
			}

			/**
			 * Accessor functions for device location
			 */
			int bus_number()      { return _bus; }
			int device_number()   { return _device; }
			int function_number() { return _function; }

			void print(Genode::Output &out) const
			{
				using Genode::print;
				using Genode::Hex;
				print(out, Hex(_bus, Hex::Prefix::OMIT_PREFIX),
				      ":", Hex(_device, Hex::Prefix::OMIT_PREFIX),
				      ".", Hex(_function, Hex::Prefix::OMIT_PREFIX));
			}

			Genode::uint16_t bdf () {
				return (_bus << 8) | (_device << 3) | (_function & 0x7); }

			/**
			 * Accessor functions for device information
			 */
			unsigned short device_id() { return _device_id; }
			unsigned short vendor_id() { return _vendor_id; }
			unsigned int  class_code() { return _class_code; }

			/**
			 * Return true if device is a PCI bridge
			 */
			bool pci_bridge() { return _header_type == HEADER_PCI_TO_PCI; }

			/**
			 * Return true if device is a PCI bridge
			 *
			 * \deprecated  use 'pci_bridge instead
			 */
			bool is_pci_bridge() { return pci_bridge(); }

			/**
			 * Return true if device is valid
			 */
			bool valid() { return _vendor_id != INVALID_VENDOR; }

			/**
			 * Return resource description by resource ID
			 */
			Device::Resource resource(int resource_id)
			{
				/* return invalid resource if sanity check fails */
				if (!_resource_id_is_valid(resource_id))
					return Device::Resource(0, 0);

				return _resource[resource_id];
			}

			/**
			 * Read configuration space
			 */
			enum { DONT_TRACK_ACCESS = false };
			unsigned read(Config_access *pci_config, unsigned char address,
			              Device::Access_size size, bool track = true)
			{
				return pci_config->read(_bus, _device, _function, address,
				                        size, track);
			}

			/**
			 * Write configuration space
			 */
			void write(Config_access *pci_config, unsigned char address,
			           unsigned long value, Device::Access_size size,
			           bool track = true)
			{
				pci_config->write(_bus, _device, _function, address, value,
				                  size, track);
			}

			bool reg_in_use(Config_access *pci_config, unsigned char address,
			                Device::Access_size size) {
				return pci_config->reg_in_use(address, size); }
	};

	class Config_space : public Genode::List<Config_space>::Element
	{
		private:

			Genode::uint32_t _bdf_start;
			Genode::uint32_t _func_count;
			Genode::addr_t   _base;

		public:

			Config_space(Genode::uint32_t bdf_start,
			             Genode::uint32_t func_count, Genode::addr_t base)
			:
				_bdf_start(bdf_start), _func_count(func_count), _base(base) {}

			Genode::addr_t lookup_config_space(Genode::uint32_t bdf)
			{
				if ((_bdf_start <= bdf) && (bdf <= _bdf_start + _func_count - 1))
					return _base + (bdf << 12);
				return 0;
			}
	};
}

#endif /* _X86__PCI_DEVICE_CONFIG_H_ */
