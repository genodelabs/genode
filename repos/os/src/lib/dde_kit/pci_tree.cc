/*
 * \brief  Virtual PCI bus tree for DDE kit
 * \author Christian Helmuth
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "pci_tree.h"

using namespace Dde_kit;

static const bool verbose = false;
static const bool verbose_access = false;

/****************
 ** PCI device **
 ****************/

uint32_t Pci_device::config_read(unsigned char address, Pci::Device::Access_size size)
{
	uint32_t result = _device.config_read(address, size);

	if (verbose_access)
		PDBG("PCI read cfg (%d) %x of %02x:%02x.%x -- %x",
		     size, address, bus(), dev(), fun(), result);

	return result;
}


void Pci_device::config_write(unsigned char address, uint32_t val,
                              Pci::Device::Access_size size)
{
	_device.config_write(address, val, size);

	if (verbose_access)
		PDBG("PCI write cfg (%d) %x (%x) of %02x:%02x.%x",
		     size, address, val, bus(), dev(), fun());
}


/*************
 ** PCI bus **
 *************/

Pci_tree::Pci_tree(unsigned device_class, unsigned class_mask)
{
	/*
	 * Iterate through all accessible devices and populate virtual
	 * PCI bus tree.
	 */
	Pci::Device_capability prev_device_cap;
	Pci::Device_capability device_cap = _pci_drv.first_device(device_class,
	                                                          class_mask);
	while (device_cap.valid()) {

		Pci_device *device = new (env()->heap()) Pci_device(device_cap);

		_devices.insert(device);

		prev_device_cap = device_cap;

		for (unsigned i = 0; i < 2; i++) {
			try {
				device_cap = _pci_drv.next_device(prev_device_cap, device_class,
				                                  class_mask);
				break;
			} catch (Pci::Device::Quota_exceeded) {
				Genode::env()->parent()->upgrade(_pci_drv.cap(), "ram_quota=4096");
			}
		}
	}

	if (verbose)
		_show_devices();

}
