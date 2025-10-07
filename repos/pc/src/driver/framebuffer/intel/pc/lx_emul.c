/*
 * \brief  Linux emulation environment specific to this driver
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2021-2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/io_mem.h>
#include <lx_emul/page_virt.h>

#include <acpi/video.h>

#include <linux/dma-fence.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include "shmem_intel.h"

#include "i915_drv.h"
#include "intel_pci_config.h"
#include "i915/gem/i915_gem_lmem.h"
#include "i915/gt/intel_gt.h"
#include <drm/drm_managed.h>


struct dma_fence_ops const i915_fence_ops;

/* Bits allowed in normal kernel mappings: */
pteval_t __default_kernel_pte_mask __read_mostly = ~0;


void si_meminfo(struct sysinfo * val)
{
	unsigned long long const ram_pages = emul_avail_ram() / PAGE_SIZE;

	/* used by drivers/gpu/drm/ttm/ttm_device.c */
	val->totalram  = ram_pages;
	val->sharedram = 0;
	val->freeram   = ram_pages;
	val->bufferram = 0;
	val->totalhigh = 0;
	val->freehigh  = 0;
	val->mem_unit  = PAGE_SIZE;

	lx_emul_trace(__func__);
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


pgprot_t pgprot_writecombine(pgprot_t prot)
{
	pgprot_t p = { .pgprot = 0 };
	lx_emul_trace(__func__);
	return p;
}


void __iomem * ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	lx_emul_trace(__func__);
	return lx_emul_io_mem_map(phys_addr, size, true);
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


void * memremap(resource_size_t offset, size_t size, unsigned long flags)
{
	lx_emul_trace(__func__);

	return intel_io_mem_map(offset, size);
}


void intel_vgpu_detect(struct drm_i915_private * dev_priv)
{
	/*
	 * We don't want to use the GPU in this display driver.
	 * By setting the ppgtt support to NONE, code paths in early driver
	 * probe/boot up are not trigged (INTEL_PPGTT_ALIASING, Lenovo T420)
	 */

	struct intel_runtime_info *rinfo = RUNTIME_INFO(dev_priv);
	rinfo->ppgtt_type = INTEL_PPGTT_NONE;

	printk("disabling PPGTT to avoid GPU code paths\n");
}


#include <linux/dma-mapping.h>

size_t dma_max_mapping_size(struct device * dev)
{
	lx_emul_trace(__func__);
	return PAGE_SIZE * 512; /* 2 MB */
}


#include <linux/uaccess.h>

unsigned long _copy_from_user(void * to, const void __user * from,
                              unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to, const void * from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


enum acpi_backlight_type __acpi_video_get_backlight_type(bool native,
                                                         bool * auto_detect)
{
	enum acpi_backlight_type const type = acpi_backlight_native;

	printk("\n%s -> %s\n", __func__,
	       type == acpi_backlight_native ? "native" :
	       type == acpi_backlight_vendor ? "vendor" : "unknown");

	return type;
}



/*
 * Very very basic folio free-up emulation
 */


void folio_mark_accessed(struct folio *folio)
{
	lx_emul_trace(__func__);
}


void check_move_unevictable_folios(struct folio_batch *fbatch)
{
	lx_emul_trace(__func__);
}


void free_huge_folio(struct folio *folio)
{
	lx_emul_trace_and_stop(__func__);
}


void folio_undo_large_rmappable(struct folio *folio)
{
	lx_emul_trace_and_stop(__func__);
}


void free_unref_page(struct page *page, unsigned int order)
{
	lx_emul_trace_and_stop(__func__);
}


/*
 * see 6.6.47 linux/src/linux/mm/page_alloc.c - mostly original code, beside __folio_put
 */
void destroy_large_folio(struct folio *folio)
{
	if (folio_test_hugetlb(folio)) {
		free_huge_folio(folio);
		return;
	}

	if (folio_test_large_rmappable(folio))
		folio_undo_large_rmappable(folio);

	mem_cgroup_uncharge(folio);

	__folio_put(folio);
}

/*
 * see 6.6.47 linux/src/linux/mm/swap.c - this is a very shorten version of it
 */
static void __page_cache_release(struct folio *folio)
{
	if (folio_test_lru(folio)) {
		lx_emul_trace_and_stop(__func__);
	}
	/* See comment on folio_test_mlocked in release_pages() */
	if (unlikely(folio_test_mlocked(folio))) {
		lx_emul_trace_and_stop(__func__);
	}
}


/*
 * see 6.6.47 linux/src/linux/mm/swap.c - original code
 */
static void __folio_put_large(struct folio *folio)
{
	/*
	 * __page_cache_release() is supposed to be called for thp, not for
	 * hugetlb. This is because hugetlb page does never have PageLRU set
	 * (it's never listed to any LRU lists) and no memcg routines should
	 * be called for hugetlb (it has a separate hugetlb_cgroup.)
	 */
	if (!folio_test_hugetlb(folio))
		__page_cache_release(folio);
	destroy_large_folio(folio);
}


/*
 * see 6.6.47 linux/src/linux/mm/swap.c - this is a very shorten version of it
 */
void release_pages(release_pages_arg arg, int nr)
{
	int i;
	struct encoded_page **encoded = arg.encoded_pages;

	for (i = 0; i < nr; i++) {
		struct folio *folio;

		/* Turn any of the argument types into a folio */
		folio = page_folio(encoded_page_ptr(encoded[i]));

		if (is_huge_zero_folio(folio))
			continue;

		if (folio_is_zone_device(folio))
			lx_emul_trace_and_stop(__func__);

		if (!folio_put_testzero(folio))
			continue;

		if (folio_test_large(folio)) {
			lx_emul_trace(__func__);
			__folio_put_large(folio);
			continue;
		}

		if (folio_test_lru(folio))
			lx_emul_trace_and_stop(__func__);

		if (unlikely(folio_test_mlocked(folio)))
			lx_emul_trace_and_stop(__func__);
	}
}


void __folio_batch_release(struct folio_batch *fbatch)
{
	lx_emul_trace(__func__);

	release_pages(fbatch->folios, folio_batch_count(fbatch));
	folio_batch_reinit(fbatch);
}


void __fix_address
sk_skb_reason_drop(struct sock *sk, struct sk_buff *skb, enum skb_drop_reason reason)
{
	if (!skb)
		return;

	if (unlikely(!skb_unref(skb)))
		return;

	printk("%s ---- LEAKING skb\n", __func__);
}
