/*
 * \brief  Linux kernel PCI
 * \author Josef Soentgen
 * \date   2022-01-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/slab.h>


#include <linux/interrupt.h>

int arch_probe_nr_irqs(void)
{
    return 16;
}


#include <asm/x86_init.h>

static int x86_init_pci_init(void)
{
	return 1;
}


static void x86_init_pci_init_irq(void) { }


struct x86_init_ops x86_init = {
	.pci = {
		.init     = x86_init_pci_init,
		.init_irq = x86_init_pci_init_irq,
	},
};


#include <lx_emul/pci_config_space.h>

#include <linux/pci.h>
#include <asm/pci.h>
#include <asm/pci_x86.h>

static int pci_raw_ops_read(unsigned int domain, unsigned int bus, unsigned int devfn,
                            int reg, int len, u32 *val)
{
	return lx_emul_pci_read_config(bus, devfn, (unsigned)reg, (unsigned)len, val);
}


static int pci_raw_ops_write(unsigned int domain, unsigned int bus, unsigned int devfn,
                             int reg, int len, u32 val)
{
	return lx_emul_pci_write_config(bus, devfn, (unsigned)reg, (unsigned)len, val);
}


const struct pci_raw_ops genode_raw_pci_ops = {
	.read  = pci_raw_ops_read,
	.write = pci_raw_ops_write,
};

const struct pci_raw_ops *raw_pci_ops = &genode_raw_pci_ops;


static int pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
    return pci_raw_ops_read(0, bus->number, devfn, where, size, value);
}


static int pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
    return pci_raw_ops_write(0, bus->number, devfn, where, size, value);
}


struct pci_ops pci_root_ops = {
    .read  = pci_read,
    .write = pci_write,
};


#include <lx_emul/init.h>

static struct resource _dummy_parent;

void pcibios_scan_root(int busnum)
{
	struct pci_bus *bus;
	struct pci_sysdata *sd;
	struct pci_dev *dev;

	LIST_HEAD(resources);

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd) {
		return;
	}
	sd->node = NUMA_NO_NODE;
	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);

	bus = pci_scan_root_bus(NULL, busnum, &pci_root_ops, sd, &resources);

	if (!bus) {
		pci_free_resource_list(&resources);
		kfree(sd);
		return;
	}
	pci_bus_add_devices(bus);

	/* handle early quirks */
	list_for_each_entry(dev, &bus->devices, bus_list) {

		/*
		 * As pci_enable_resources() is only going to check if
		 * the parent of the resource is set register the dummy.
		 */
		struct resource *r;
		int i;
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {

			r = &dev->resource[i];
			r->parent = &_dummy_parent;
		}

		lx_emul_execute_pci_fixup(dev);
	}
}


#include <linux/irq.h>
#include <linux/pci.h>

extern struct irq_chip dde_irqchip_data_chip;


void pci_assign_irq(struct pci_dev * dev)
{
	struct irq_data *irq_data;

	/*
	 * Be lazy and treat irq as hwirq as this is used by the
	 * dde_irqchip_data_chip for (un-)masking.
	 */
	irq_data = irq_get_irq_data(dev->irq);
	irq_data->hwirq = dev->irq;

	irq_set_chip_and_handler(dev->irq, &dde_irqchip_data_chip,
	                         handle_level_irq);
}


#include <linux/pci.h>

unsigned long pci_mem_start = 0xaeedbabe;

const struct attribute_group aspm_ctrl_attr_group[] = { 0 };
const struct attribute_group pci_dev_vpd_attr_group = { };

struct pci_fixup __start_pci_fixups_early[] = { 0 };
struct pci_fixup __end_pci_fixups_early[] = { 0 };
struct pci_fixup __start_pci_fixups_header[] = { 0 };
struct pci_fixup __end_pci_fixups_header[] = { 0 };
struct pci_fixup __start_pci_fixups_final[] = { 0 };
struct pci_fixup __end_pci_fixups_final[] = { 0 };
struct pci_fixup __start_pci_fixups_enable[] = { 0 };
struct pci_fixup __end_pci_fixups_enable[] = { 0 };
struct pci_fixup __start_pci_fixups_resume[] = { 0 };
struct pci_fixup __end_pci_fixups_resume[] = { 0 };
struct pci_fixup __start_pci_fixups_resume_early[] = { 0 };
struct pci_fixup __end_pci_fixups_resume_early[] = { 0 };
struct pci_fixup __start_pci_fixups_suspend[] = { 0 };
struct pci_fixup __end_pci_fixups_suspend[] = { 0 };
struct pci_fixup __start_pci_fixups_suspend_late[] = { 0 };
struct pci_fixup __end_pci_fixups_suspend_late[] = { 0 };

int pcibios_last_bus = -1;


extern int __init pcibios_init(void);
int __init pcibios_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}
