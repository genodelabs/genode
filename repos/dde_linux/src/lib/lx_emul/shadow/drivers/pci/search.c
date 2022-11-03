/*
 * \brief  Replaces drivers/pci/search.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/pci.h>
#include <linux/pci.h>

#include "../drivers/pci/pci.h"

struct pci_dev * pci_get_class(unsigned int class, struct pci_dev *from)
{
	struct pci_dev *dev;
	struct pci_bus *bus = (struct pci_bus *) lx_emul_pci_root_bus();

	/*
	 * Break endless loop (see 'intel_dsm_detect()') by only querying
	 * the bus on the first executuin.
	 */
	if (from)
		return NULL;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->class == class) {
			return dev;
		}
	}

	return NULL;
}


struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
                                            unsigned int devfn)
{
	struct pci_dev *dev;
	struct pci_bus *pbus = (struct pci_bus *) lx_emul_pci_root_bus();

	list_for_each_entry(dev, &pbus->devices, bus_list) {
		if (dev->devfn == devfn && dev->class == 0x60000)
			return dev;
	}

	return NULL;
}


struct pci_dev * pci_get_device(unsigned int vendor,unsigned int device,struct pci_dev * from)
{
	return NULL;
}


int pci_dev_present(const struct pci_device_id *ids)
{
	struct pci_dev *dev;
	struct pci_bus *bus = (struct pci_bus *) lx_emul_pci_root_bus();

	while (ids->vendor || ids->subvendor || ids->class_mask) {
		list_for_each_entry(dev, &bus->devices, bus_list) {
			if (pci_match_one_device(ids, dev))
				return 1;
		}
		ids++;
	}

	return 0;
}
