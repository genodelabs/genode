/*
 * \brief  Additional PCI functions needed by Intel graphics driver
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2022-07-27
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

#include <linux/pci.h>
#include <drm/i915_drm.h>
#include <../drivers/gpu/drm/i915/intel_pci_config.h>
#undef GFX_FLSH_CNTL /* suppress warning of double define */
#include <../drivers/char/agp/intel-agp.h>


unsigned long pci_mem_start = 0xaeedbabe;


int pci_bus_alloc_resource(struct pci_bus *bus, struct resource *res,
        resource_size_t size, resource_size_t align,
        resource_size_t min, unsigned long type_mask,
        resource_size_t (*alignf)(void *,
                      const struct resource *,
                      resource_size_t,
                      resource_size_t),
        void *alignf_data)
{
	return -1;
}


void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	struct resource *r;
	unsigned long phys_addr;
	unsigned long size;

	if (!dev || bar > 5) {
		printk("%s:%d: invalid request for dev: %p bar: %d\n",
		       __func__, __LINE__, dev, bar);
		return NULL;
	}

	printk("pci_iomap: request for dev: %s bar: %d\n", dev_name(&dev->dev), bar);

	r = &dev->resource[bar];

	phys_addr = r->start;
	size      = r->end - r->start;

	if (!phys_addr || !size)
		return NULL;

	return lx_emul_io_mem_map(phys_addr, size);
}


void __iomem * pci_map_rom(struct pci_dev * pdev,size_t * size)
{
	/*
	 * Needed for VBT access which we do not allow
	 */
	return NULL;
}


int pci_read_config_word(const struct pci_dev * dev, int where, u16 *val)
{
	switch (where) {
	/* Intel graphics and memory controller hub control register */
	/* I830_GMCH_CTRL is identical to INTEL_GMCH_CTRL */
	case SNB_GMCH_CTRL:
	case INTEL_GMCH_CTRL:
		*val = emul_intel_gmch_control_reg();
		return 0;
	case SWSCI: /* intel_fb: software smi sci */
		*val = 0;
		return 0;
	};

	printk("%s: %s %d %d\n", __func__, dev_name(&dev->dev), dev->devfn, where);
	lx_emul_trace_and_stop(__func__);
}


int pci_read_config_dword(const struct pci_dev * dev, int where, u32 *val)
{
	switch (where) {
	case MCHBAR_I915:
	case MCHBAR_I965:
		*val = 0x1; /* return ENABLE bit being set */
		return 0;
	case I965_IFPADDR:     /* intel host bridge flush page (lower) */
	case I965_IFPADDR + 4: /* intel host bridge flush page (higher) */
		*val = 0;
		return 0;
	case ASLS:
		/*
		 * we just use a physical address as token here,
		 * hopefully it never crashes with other I/O memory addresses
		 */
		*val = OPREGION_PSEUDO_PHYS_ADDR;
		return 0;
	};

	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_word(const struct pci_dev * dev, int where, u16 val)
{
	switch (where) {
	case SWSCI: /* intel_fb: software smi sci */
		/* just ignore */
		return 0;
	};
	lx_emul_trace_and_stop(__func__);
}


int pci_write_config_dword(const struct pci_dev * dev, int where, u32 val)
{
	switch (where) {
	case I965_IFPADDR:     /* intel host bridge flush page (lower) */
	case I965_IFPADDR + 4: /* intel host bridge flush page (higher) */
		/* just ignore */
		return 0;
	};
	lx_emul_trace_and_stop(__func__);
}
