/*
 * \brief  Implementation of linux/pci.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <lx_kit/env.h>
#include <lx_kit/pci_dev_registry.h>
#include <lx_kit/mapped_io_mem_range.h>

#include <lx_emul/impl/pci_resource.h>


extern "C" int pci_register_driver(struct pci_driver *driver)
{
	driver->driver.name = driver->name;

	pci_device_id const *id_table = driver->id_table;
	if (!id_table)
		return -ENODEV;

	using namespace Genode;

	Lx::Pci_dev *pci_dev = nullptr;

	auto lamda = [&] (Platform::Device_capability cap) {

		Platform::Device_client client(cap);

		/* request device ID from platform driver */
		unsigned const device_id = client.device_id();

		/* look if we find the device ID in the driver's 'id_table' */
		pci_device_id const *matching_id = nullptr;
		for (pci_device_id const *id = id_table; id->device; id++) {
			if (id->device == device_id)
				matching_id = id;
		}

		/* skip device that is not handled by driver */
		if (!matching_id)
			return false;

		/* create 'pci_dev' struct for matching device */
		pci_dev = new (&Lx_kit::env().heap()) Lx::Pci_dev(cap);

		/* enable ioremap to work */
		Lx::pci_dev_registry()->insert(pci_dev);

		/* register driver at the 'pci_dev' struct */
		pci_dev->dev.driver = &driver->driver;

		/* call probe function of the Linux driver */
		if (driver->probe(pci_dev, matching_id)) {

			/* if the probing failed, revert the creation of 'pci_dev' */
			pci_dev_put(pci_dev);
			pci_dev = nullptr;
			return false;
		}

		/* aquire the device, stop iterating */
		return true;
	};

	Lx::for_each_pci_device(lamda);

	return pci_dev ? 0 : -ENODEV;
}
