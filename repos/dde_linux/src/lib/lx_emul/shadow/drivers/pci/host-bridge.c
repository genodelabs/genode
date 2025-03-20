/*
 * \brief  PCI-host bridge helpers
 * \author Stefan Kalkowksi
 * \date   2022-07-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/pci.h>

void pcibios_resource_to_bus(struct pci_bus *bus, struct pci_bus_region *region,
                             struct resource *res)
{
	region->start = res->start;
	region->end = res->end;
}


void pci_put_host_bridge_device(struct device * dev) { }
