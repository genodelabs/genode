#include <linux/pci.h>

void pcibios_resource_to_bus(struct pci_bus *bus, struct pci_bus_region *region,
                             struct resource *res)
{
	region->start = res->start;
	region->end = res->end;
}


void pci_put_host_bridge_device(struct device * dev) { }
