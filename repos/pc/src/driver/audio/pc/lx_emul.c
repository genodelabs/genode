/*
 * \brief  Genode implementation of Linux kernel functions
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/async.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>

const struct attribute_group hdac_dev_attr_groups;
const struct attribute_group pci_dev_acpi_attr_group;
const struct attribute_group input_poller_attribute_group;

pteval_t __default_kernel_pte_mask __read_mostly = ~0;

/*
 * vmap
 */
void * vmap(struct page ** pages,unsigned int count, unsigned long flags, pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


void vunmap(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


/*
 * module_params
 */
extern char **module_param_hda_model(void);

void lx_emul_module_params(void)
{
	//XXX: make configurable
	char **hda_model = module_param_hda_model();
	*hda_model = "dell-headset-multi";
}


/*
 * uaccess
 */
unsigned long _copy_to_user(void __user * to, const void * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


unsigned long _copy_from_user(void * to, const void __user * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


void __iomem * pci_ioremap_bar(struct pci_dev *pdev, int bar)
{
	struct resource *res = &pdev->resource[bar];
	return ioremap(res->start, resource_size(res));
}


void __iomem * pcim_iomap_region(struct pci_dev * pdev,int bar,const char * name)
{
	return pci_ioremap_bar(pdev, bar);
}


int pci_read_config_byte(const struct pci_dev * dev,int where,u8 * val)
{
	printk("%s: where=0x%x\n", __func__, where);
	*val = 0;
	return 0;
}


int pci_read_config_dword(const struct pci_dev *dev, int where, u32 *val)
{
	printk("%s: where=0x%x\n", __func__, where);
	*val = 0;
	return 0;
}


int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val)
{
	printk("%s: where=0x%x\n", __func__, where);
	*val = 0;
	return 0;
}


int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	printk("%s: where=0x%x\n", __func__, where);
	return 0;
}


int pci_write_config_dword(const struct pci_dev *dev, int where, u32 val)
{
	printk("%s: where=0x%x\n", __func__, where);
	return 0;
}


int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
                                   unsigned int max_vecs, unsigned int flags,
                                   struct irq_affinity *aff_desc)
{
	/* check for legacy IRQ */
	if ((flags & PCI_IRQ_INTX) && min_vecs == 1 && dev->irq)
		return 1;
	return -ENOSPC;
}


int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
                          unsigned int max_vecs, unsigned int flags)
{
	return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs, flags, NULL);
}


int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (WARN_ON_ONCE(nr > 0))
		return -EINVAL;
	return dev->irq;
}


void pci_free_irq_vectors(struct pci_dev *dev)
{
}


/*
 * dma mapping
 */

struct sg_table * dma_alloc_noncontiguous(struct device * dev, size_t size,
                                          enum dma_data_direction dir, gfp_t gfp,
                                          unsigned long attrs)
{
	struct sg_table    *sgt;
	struct scatterlist *sgl;

	if (WARN_ON_ONCE(attrs & ~DMA_ATTR_ALLOC_SINGLE_PAGES))
		return NULL;

	sgt = kmalloc(sizeof(*sgt), gfp);
	if (!sgt) return NULL;

	sgl = kmalloc(sizeof(*sgl), gfp);
	if (!sgl) {
		kfree(sgt);
		return NULL;
	}

	/* alloc contiguous ;) */
	sgl->page_link = (unsigned long) alloc_pages(0, get_order(size));
	if (!sgl->page_link) goto error;

	sgl->page_link |= SG_END;
	sgl->offset     = 0;
	sgl->length     = size;

	if (!dma_map_sg_attrs(NULL, sgl, 1, dir, attrs)) goto error;

	sgt->sgl        = sgl;
	sgt->nents      = 1;
	sgt->orig_nents = 1;

	return sgt;

error:
	kfree(sgl);
	kfree(sgt);
	return NULL;
}


void dma_free_noncontiguous(struct device * dev, size_t size,
                            struct sg_table * sgt, enum dma_data_direction dir)
{
	dma_unmap_sg_attrs(NULL, sgt->sgl, sgt->nents, dir, 0);

	__free_pages(sg_page(sgt->sgl), get_order(sgt->sgl->length));

	kfree(sgt->sgl);
	kfree(sgt);
}


void * dma_vmap_noncontiguous(struct device * dev, size_t size, struct sg_table * sgt)
{
	if (!sgt || !sgt->sgl) return NULL;
	return page_address(sg_page(sgt->sgl));
}


void dma_vunmap_noncontiguous(struct device * dev, void * vaddr)
{
	lx_emul_trace(__func__);
}


#ifndef INLINE_COPY_TO_USER
unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


#ifndef INLINE_COPY_FROM_USER
unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif
