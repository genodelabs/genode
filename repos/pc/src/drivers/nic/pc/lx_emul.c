/*
 * \brief  Linux emulation environment specific to this driver
 * \author Christian Helmuth
 * \date   2023-05-22
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>


unsigned long __FIXADDR_TOP = 0xfffff000;


#include <linux/uaccess.h>

#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_from_user(void * to, const void __user * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#include <linux/gfp.h>

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)__alloc_pages(GFP_KERNEL, 0, 0, NULL)->virtual;
}

void * page_frag_alloc_align(struct page_frag_cache *nc,
                             unsigned int fragsz, gfp_t gfp_mask,
                             unsigned int align_mask)
{
	struct page *page;

	if (fragsz > PAGE_SIZE) {
		printk("no support for fragments larger than PAGE_SIZE\n");
		lx_emul_trace_and_stop(__func__);
	}

	page = __alloc_pages(gfp_mask, 0, 0, NULL);

	if (!page)
		return NULL;

	return page->virtual;
}

void page_frag_free(void * addr)
{
	struct page *page = lx_emul_virt_to_page(addr);
	if (!page) {
		printk("BUG %s: page for addr: %p not found\n", __func__, addr);
		lx_emul_backtrace();
	}

	__free_pages(page, 0ul);
}


#include <linux/slab.h>

struct kmem_cache * kmem_cache_create_usercopy(const char * name,
                                               unsigned int size,
                                               unsigned int align,
                                               slab_flags_t flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (* ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}

int kmem_cache_alloc_bulk(struct kmem_cache * s,gfp_t flags, size_t nr,void ** p)
{
	size_t i;
	for (i = 0; i < nr; i++)
		p[i] = kmem_cache_alloc(s, flags);

	return nr;
}


void kmem_cache_free_bulk(struct kmem_cache * s, size_t size, void ** p)
{
	size_t i;

	for (i = 0; i < size; i++)
		kmem_cache_free(s, p[i]);
}


#include <asm/hardirq.h>

void ack_bad_irq(unsigned int irq)
{
	printk(KERN_CRIT "unexpected IRQ trap at vector %02x\n", irq);
}


#include <linux/pci.h>

void __iomem * pci_ioremap_bar(struct pci_dev * pdev, int bar)
{
	struct resource *res = &pdev->resource[bar];
	return ioremap(res->start, resource_size(res));
}

int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val)
{
	switch (where) {
	case PCI_COMMAND:
		*val = 0x7;
		return 0;

	/*
	 * drivers/net/ethernet/intel/e1000e/ich8lan.c e1000_platform_pm_pch_lpt
	 */
	case 0xa8:
	case 0xaa:
		*val = 0;
		return 0;
	/*
	 * drivers/net/ethernet/intel/e1000e/netdev.c e1000_flush_desc_rings
	 *
	 * In i219, the descriptor rings must be emptied before resetting the HW or
	 * before changing the device state to D3 during runtime (runtime PM).
	 *
	 * Failure to do this will cause the HW to enter a unit hang state which
	 * can only be released by PCI reset on the device
	 */
	case 0xe4:
		/* XXX report no need to flush */
		*val = 0;
		return 0;
	};

	printk("%s: unexpected read at %x\n", __func__, where);
	PCI_SET_ERROR_RESPONSE(val);;
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}

int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	PCI_SET_ERROR_RESPONSE(val);;
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_capability_write_word(struct pci_dev * dev, int pos, u16 val)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_capability_clear_and_set_word(struct pci_dev *dev, int pos, u16 clear, u16 set)
{
	printk("%s: unsupported pos=%x\n", __func__, pos);
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


int pcie_set_readrq(struct pci_dev * dev, int rq)
{
	printk("%s: unsupported rq=%d\n", __func__, rq);
	return pcibios_err_to_errno(PCIBIOS_FUNC_NOT_SUPPORTED);
}


static unsigned long *_pci_iomap_table;

void __iomem * const * pcim_iomap_table(struct pci_dev * pdev)
{
	unsigned i;

	if (!_pci_iomap_table)
		_pci_iomap_table = kzalloc(sizeof (unsigned long*) * 6, GFP_KERNEL);

	if (!_pci_iomap_table)
		return NULL;

	for (i = 0; i < 6; i++) {
		struct resource *r = &pdev->resource[i];
		unsigned long phys_addr = r->start;
		unsigned long size      = r->end - r->start;

		if (!(r->flags & IORESOURCE_MEM))
			continue;

		if (!phys_addr || !size)
			continue;

		_pci_iomap_table[i] =
			(unsigned long)lx_emul_io_mem_map(phys_addr, size);
	}

	return (void const *)_pci_iomap_table;
}


int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	int bars = 0;
	unsigned long const *table;
	unsigned i;

	if (!(flags & IORESOURCE_MEM))
		return 0;

	/* misuse 'pcim_iomap_table()' for querying I/O mem */
	table = (unsigned long const *)pcim_iomap_table(dev);

	for (i = 0; i < 6; i++)
		if (table[i])
			bars |= (1 << i);

	return bars;
}


int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
                                   unsigned int max_vecs, unsigned int flags,
                                   struct irq_affinity *aff_desc)
{
	if ((flags & PCI_IRQ_LEGACY) && min_vecs == 1 && dev->irq)
		return 1;
	return -ENOSPC;
}


int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (WARN_ON_ONCE(nr > 0))
		return -EINVAL;
	return dev->irq;
}


#include <linux/dma-mapping.h>

void *dmam_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma_handle,
                       gfp_t gfp, unsigned long attrs)
{
	return dma_alloc_attrs(dev, size, dma_handle, gfp, attrs);
}
