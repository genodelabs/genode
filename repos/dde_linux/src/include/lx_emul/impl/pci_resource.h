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
#include <lx_kit/pci_dev_registry.h>
#include <lx_kit/mapped_io_mem_range.h>


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
