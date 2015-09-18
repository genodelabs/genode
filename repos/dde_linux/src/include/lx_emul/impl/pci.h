/*
 * \brief  Implementation of linux/pci.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <io_mem_session/connection.h>

#include <lx_emul/impl/internal/pci_dev_registry.h>
#include <lx_emul/impl/internal/mapped_io_mem_range.h>


extern void pci_dev_put(struct pci_dev *pci_dev)
{
	Genode::destroy(Genode::env()->heap(), pci_dev);
}


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
		for (pci_device_id const *id = id_table; id->class_ != 0; id++)
			if (id->device == device_id)
				matching_id = id;

		/* skip device that is not handled by driver */
		if (!matching_id)
			return false;

		/* create 'pci_dev' struct for matching device */
		pci_dev = new (env()->heap()) Lx::Pci_dev(cap);

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


extern "C" size_t pci_resource_start(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].start;
}

extern "C" size_t pci_resource_end(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].end;
}

extern "C" size_t pci_resource_len(struct pci_dev *dev, unsigned bar)
{
	size_t start = pci_resource_start(dev, bar);

	if (!start)
		return 0;

	return (dev->resource[bar].end - start) + 1;
}


extern "C" void *pci_ioremap_bar(struct pci_dev *dev, int bar)
{
	using namespace Genode;

	if (bar >= DEVICE_COUNT_RESOURCE || bar < 0)
		return 0;

	return Lx::ioremap(pci_resource_start(dev, bar),
	                   pci_resource_len(dev, bar),
	                   Genode::UNCACHED);
}


extern "C" unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar)
{
	if (bar >= DEVICE_COUNT_RESOURCE)
		return 0;

	return dev->resource[bar].flags;
}


extern "C" int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int, int where, u8 *val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_read(where, val);
	return 0;
}


extern "C" int pci_bus_read_config_word(struct pci_bus *bus, unsigned int, int where, u16 *val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_read(where, val);
	return 0;
}


extern "C" int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int, int where, u32 *val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_read(where, val);
	return 0;
}


extern "C" int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int, int where, u8 val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_write(where, val);
	return 0;
}


extern "C" int pci_bus_write_config_word(struct pci_bus *bus, unsigned int, int where, u16 val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_write(where, val);
	return 0;
}


extern "C" int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int, int where, u32 val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)bus;
	dev->config_write(where, val);
	return 0;
}


extern "C" const char *pci_name(const struct pci_dev *pdev)
{
	/* simply return driver name */
	return "dummy";
}


extern "C" int pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val)
{
	Lx::Pci_dev *dev = (Lx::Pci_dev *)pdev->bus;
	switch (pos) {
	case PCI_EXP_LNKCTL:
		dev->config_read(pdev->pcie_cap + PCI_EXP_LNKCTL, val);
		return 0;
		break;
	default:
		break;
	}

	return 1;
}
//
