/*
 * \brief  PCI backend
 * \author Josef Soentgen
 * \date   2022-05-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>
#include <lx_emul/pci.h>

#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/interrupt.h>


int arch_probe_nr_irqs(void)
{
	/* needed for 'irq_get_irq_data()' in 'pci_assign_irq()' below */
    return 256;
}


static const struct attribute_group *pci_dev_attr_groups[] = {
    NULL,
};


const struct device_type pci_dev_type = {
    .groups = pci_dev_attr_groups,
};


extern const struct device_type pci_dev_type;


static struct pci_bus *_pci_bus;


void * lx_emul_pci_root_bus()
{
	return _pci_bus;
}


static struct device * host_bridge_device = NULL;
extern struct device * pci_get_host_bridge_device(struct pci_dev * dev);
struct device * pci_get_host_bridge_device(struct pci_dev * dev)
{
	return host_bridge_device;
}


static struct pci_dev *_pci_alloc_dev(struct pci_bus *bus)
{
    struct pci_dev *dev;

    dev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
    if (!dev)
        return NULL;

    INIT_LIST_HEAD(&dev->bus_list);
    dev->dev.type = &pci_dev_type;
    dev->bus = bus;

    return dev;
}


static void pci_add_resource_to_device_callback(void        * data,
                                                unsigned      number,
                                                unsigned long addr,
                                                unsigned long size,
                                                int           io_port)
{
	struct pci_dev        * dev = (struct pci_dev*) data;
	dev->resource[number].start = addr;
	dev->resource[number].end   = dev->resource[number].start + size - 1;
	if (io_port)
		dev->resource[number].flags |= IORESOURCE_IO;
	else
		dev->resource[number].flags |= IORESOURCE_MEM;
}


static void pci_add_single_device_callback(void       * data,
                                           unsigned     number,
                                           char const * const name,
                                           u16          vendor_id,
                                           u16          device_id,
                                           u16          sub_vendor,
                                           u16          sub_device,
                                           u32          class_code,
                                           u8           revision,
                                           unsigned     irq)
{
	struct pci_bus * bus = (struct pci_bus*) data;
	struct pci_dev * dev = _pci_alloc_dev(bus);

	if (!dev) {
		printk("Error: out of memory, cannot allocate pci device %s\n", name);
		return;
	}

	dev->devfn            = number * 8;
	dev->vendor           = vendor_id;
	dev->device           = device_id;
	dev->subsystem_vendor = sub_vendor;
	dev->subsystem_device = sub_device;
	dev->irq              = irq;
	dev->dma_mask         = 0xffffffff;
	dev->dev.bus          = &pci_bus_type;
	dev->revision         = revision;
	dev->class            = class_code;
	dev->current_state    = PCI_UNKNOWN;
	dev->error_state      = pci_channel_io_normal;

	lx_emul_pci_for_each_resource(name, dev,
	                              pci_add_resource_to_device_callback);

	list_add_tail(&dev->bus_list, &bus->devices);

	device_initialize(&dev->dev);

	dev_set_name(&dev->dev, name);
	dev->dev.dma_mask = &dev->dma_mask;

	if (number == 0) { /* host bridge */
		host_bridge_device = &dev->dev;
		bus->bridge = &dev->dev;
	}

	dev->match_driver = false;
	if (device_add(&dev->dev)) {
		list_del(&dev->bus_list);
		kfree(dev);
		printk("Error: could not add pci device %s\n", name);
		return;
	}

	lx_emul_execute_pci_fixup(dev);

	dev->match_driver = true;
	if (device_attach(&dev->dev)) {
		list_del(&dev->bus_list);
		kfree(dev);
		printk("Error: could not attach pci device %s\n", name);
		return;
	}
}


static int __init pci_subsys_init(void)
{
	struct pci_bus *b;

	/* pci_alloc_bus(NULL) */
	b = kzalloc(sizeof (struct pci_bus), GFP_KERNEL);
	if (!b)
		return -ENOMEM;

#ifdef CONFIG_X86
	{
		struct pci_sysdata *sd;
		sd = kzalloc(sizeof (struct pci_sysdata), GFP_KERNEL);
		if (!sd) {
			kfree(b);
			return -ENOMEM;
		}

		/* needed by intel_fb */
		sd->domain = 0;

		b->sysdata = sd;
	}
#endif /* CONFIG_X86 */

	INIT_LIST_HEAD(&b->node);
	INIT_LIST_HEAD(&b->children);
	INIT_LIST_HEAD(&b->devices);
	INIT_LIST_HEAD(&b->slots);
	INIT_LIST_HEAD(&b->resources);
	b->max_bus_speed = PCI_SPEED_UNKNOWN;
	b->cur_bus_speed = PCI_SPEED_UNKNOWN;

	_pci_bus = b;

	/* add host bridge device */
	pci_add_single_device_callback(b, 0, "00:00.0", 0x8086, 0x44, 0x17aa, 0x2193, 0x60000, 2, 0);

	/* attach PCI devices */
	lx_emul_pci_for_each_device(b, pci_add_single_device_callback);
	return 0;
}


#ifdef CONFIG_X86
subsys_initcall(pci_subsys_init);
#else
static int __init pci_proc_init(void)
{
	return pci_subsys_init();
}

device_initcall(pci_proc_init);
#endif
