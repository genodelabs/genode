#ifndef _INCLUDE__PCIACCESS_H_
#define _INCLUDE__PCIACCESS_H_

#include <genode_types.h>

struct pci_mem_region
{
	void  *memory;
	size_t size;
};

struct pci_device
{
	struct pci_mem_region regions[6];
};

int  pci_system_init(void);
void pci_system_cleanup(void);
int  pci_device_probe(struct pci_device *dev);

struct pci_device *pci_device_find_by_slot(uint32_t domain, uint32_t bus,
                                           uint32_t dev, uint32_t func);
#endif /* _INCLUDE__PCIACCESS_H_ */
