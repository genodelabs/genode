/*
 * \brief  Linux emulation environment specific to this driver
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>

#include <linux/dma-fence.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include "i915_drv.h"
#include <drm/drm_managed.h>


/* support for arch/x86/lib/delay.c, normally defined in init/main.c */
unsigned long loops_per_jiffy = (1<<12);


struct dma_fence_ops const i915_fence_ops;

/* early_identify_cpu() in linux sets this up normally, used by drm_cache */
struct cpuinfo_x86 boot_cpu_data =
{
	.x86_clflush_size    = (sizeof(void*) == 8) ? 64 : 32,
	.x86_cache_alignment = (sizeof(void*) == 8) ? 64 : 32,
	.x86_phys_bits       = (sizeof(void*) == 8) ? 36 : 32,
	.x86_virt_bits       = (sizeof(void*) == 8) ? 48 : 32
};

/* Bits allowed in normal kernel mappings: */
pteval_t __default_kernel_pte_mask __read_mostly = ~0;


int acpi_disabled = 0;


void intel_wopcm_init_early(struct intel_wopcm * wopcm)
{
	wait_bit_init();
	lx_emul_trace(__func__);
}


void * kmalloc_order(size_t size,gfp_t flags, unsigned int order)
{
	lx_emul_trace(__func__);
	return kmalloc(size, flags);
}


int simple_pin_fs(struct file_system_type *type, struct vfsmount ** mount, int * count)
{
	lx_emul_trace(__func__);

	if (!mount)
		return -EFAULT;

	if (!*mount)
		*mount = kzalloc(sizeof(struct vfsmount), GFP_KERNEL);

	if (!*mount)
		return -ENOMEM;

	if (count)
		++*count;

	return 0;
}


struct inode * alloc_anon_inode(struct super_block * s)
{
	lx_emul_trace(__func__);

	return kzalloc(sizeof(struct inode), GFP_KERNEL);
}


struct proc_dir_entry { char dummy [512]; };

struct proc_dir_entry * proc_create_seq_private(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct seq_operations * ops,unsigned int state_size,void * data)
{
	static struct proc_dir_entry ret;

	lx_emul_trace(__func__);
	return &ret;
}


void si_meminfo(struct sysinfo * val)
{
	/* 4M, ttm actually use only half - more or configurable XXX ? */
	unsigned const ram_pages = 1024;

	/* used by drivers/gpu/drm/ttm/ttm_device.c */

	lx_emul_trace(__func__);

	val->totalram  = ram_pages;
	val->sharedram = 0;
	val->freeram   = ram_pages;
	val->bufferram = 0;
	val->totalhigh = 0;
	val->freehigh  = 0;
	val->mem_unit  = PAGE_SIZE;
}



void __iomem * ioremap(resource_size_t phys_addr, unsigned long size)
{
	return lx_emul_io_mem_map(phys_addr, size);
}


int dma_supported(struct device * dev, u64 mask)
{
	lx_emul_trace(__func__);
	return 1;
}


void yield()
{
	lx_emul_task_schedule(false /* no block */);
}


int fb_get_options(const char * name,char ** option)
{
	lx_emul_trace(__func__);

	if (!option)
		return 1;

	*option = "";

	return 0;
}


void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace(__func__);
}


bool pat_enabled(void)
{
	lx_emul_trace(__func__);
	return true;
}


pgprot_t pgprot_writecombine(pgprot_t prot)
{
	pgprot_t p = { .pgprot = 0 };
	lx_emul_trace(__func__);
	return p;
}


/*
 * shmem handling as done by Josef etnaviv
 */
#include <linux/shmem_fs.h>

struct shmem_file_buffer
{
	void        *addr;
	struct page *pages;
};


struct file *shmem_file_setup(char const *name, loff_t size,
                              unsigned long flags)
{
	struct file *f;
	struct inode *inode;
	struct address_space *mapping;
	struct shmem_file_buffer *private_data;
	loff_t const nrpages = (size / PAGE_SIZE) + ((size % (PAGE_SIZE)) ? 1 : 0);

	f = kzalloc(sizeof (struct file), 0);
	if (!f) {
		return (struct file*)ERR_PTR(-ENOMEM);
	}

	inode = kzalloc(sizeof (struct inode), 0);
	if (!inode) {
		goto err_inode;
	}

	mapping = kzalloc(sizeof (struct address_space), 0);
	if (!mapping) {
		goto err_mapping;
	}

	private_data = kzalloc(sizeof (struct shmem_file_buffer), 0);
	if (!private_data) {
		goto err_private_data;
	}

	private_data->addr = emul_alloc_shmem_file_buffer(nrpages * PAGE_SIZE);
	if (!private_data->addr)
		goto err_private_data_addr;

	/*
	 * We call virt_to_pages eagerly here, to get continuous page
	 * objects registered in case one wants to use them immediately.
	 */
	private_data->pages =
		lx_emul_virt_to_pages(private_data->addr, nrpages);

	mapping->private_data = private_data;
	mapping->nrpages = nrpages;

	inode->i_mapping = mapping;

	atomic_long_set(&f->f_count, 1);
	f->f_inode    = inode;
	f->f_mapping  = mapping;
	f->f_flags    = flags;
	f->f_mode     = OPEN_FMODE(flags);
	f->f_mode    |= FMODE_OPENED;

	return f;

err_private_data_addr:
	kfree(private_data);
err_private_data:
	kfree(mapping);
err_mapping:
	kfree(inode);
err_inode:
	kfree(f);
	return (struct file*)ERR_PTR(-ENOMEM);
}


struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
                                         pgoff_t index, gfp_t gfp)
{
	struct page *p;
	struct shmem_file_buffer *private_data;

	if (index > mapping->nrpages)
		return NULL;

	private_data = mapping->private_data;

	p = private_data->pages;
	return (p + index);
}


#ifdef CONFIG_SWIOTLB

#include <linux/swiotlb.h>

unsigned int swiotlb_max_segment(void)
{
	lx_emul_trace(__func__);
	return PAGE_SIZE * 512;
}


bool is_swiotlb_active(void)
{
	lx_emul_trace(__func__);
	return false;
}

#endif


/* linux/mm/page_alloc.c */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = alloc_pages(gfp_mask & ~__GFP_HIGHMEM, order);

	if (!page)
		return 0;

	return (unsigned long) page_address(page);
}


/* linux/mm/page_alloc.c */
void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		__free_pages(virt_to_page((void *)addr), order);
	}
}


void intel_gt_init_early(struct intel_gt * gt, struct drm_i915_private * i915)
{
	gt->i915 = i915;
	gt->uncore = &i915->uncore;

	spin_lock_init(&gt->irq_lock);

	INIT_LIST_HEAD(&gt->closed_vma);
	spin_lock_init(&gt->closed_lock);

	init_llist_head(&gt->watchdog.list);

	lx_emul_trace(__func__);
}


void __iomem * ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	lx_emul_trace(__func__);
	return lx_emul_io_mem_map(phys_addr, size);
}


int iomap_create_wc(resource_size_t base, unsigned long size, pgprot_t *prot)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_rps_mark_interactive(struct intel_rps * rps, bool interactive)
{
	lx_emul_trace(__func__);
}


bool is_vmalloc_addr(const void * addr)
{
	lx_emul_trace(__func__);
	return false;
}


void * memremap(resource_size_t offset, size_t size, unsigned long flags)
{
	lx_emul_trace(__func__);

	return NULL;
}


void intel_vgpu_detect(struct drm_i915_private * dev_priv)
{
	/*
	 * We don't want to use the GPU in this display driver.
	 * By setting the ppgtt support to NONE, code paths in early driver
	 * probe/boot up are not trigged (INTEL_PPGTT_ALIASING, Lenovo T420)
	 */

	struct intel_device_info *info = mkwrite_device_info(dev_priv);
	info->ppgtt_type = INTEL_PPGTT_NONE;

	printk("disabling PPGTT to avoid GPU code paths\n");
}


void call_rcu(struct rcu_head * head, rcu_callback_t func)
{
	lx_emul_trace(__func__);

	func(head);
}
