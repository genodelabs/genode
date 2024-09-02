/*
 * \brief  Linux emulation environment specific to this driver
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2021-2024 Genode Labs GmbH
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


int intel_root_gt_init_early(struct drm_i915_private * i915)
{
	struct intel_gt *gt = to_gt(i915);

	gt->i915 = i915;
	gt->uncore = &i915->uncore;
	gt->irq_lock = drmm_kzalloc(&i915->drm, sizeof(*gt->irq_lock), GFP_KERNEL);
	if (!gt->irq_lock)
		return -ENOMEM;

	spin_lock_init(gt->irq_lock);

	INIT_LIST_HEAD(&gt->closed_vma);
	spin_lock_init(&gt->closed_lock);

	init_llist_head(&gt->watchdog.list);

	mutex_init(&gt->tlb.invalidate_lock);
	seqcount_mutex_init(&gt->tlb.seqno, &gt->tlb.invalidate_lock);

	intel_uc_init_early(&gt->uc);

	/*
	 * Tells driver that IOMMU, e.g. VT-d, is on, so that scratch page
	 * workaround is applied by Intel display driver:
	 *
	 * drivers/gpu/drm/i915/gt/intel_ggtt.c
	 *  -> gen8_gmch_probe() -> intel_scanout_needs_vtd_wa(i915)
	 *  ->    return DISPLAY_VER(i915) >= 6 && i915_vtd_active(i915);
	 *
	 * i915_vtd_active() uses
	 *   if (device_iommu_mapped(i915->drm.dev))
	 *     return true;
	 *
	 *   which checks for dev->iommu_group != NULL
	 *
	 * The struct iommu_group is solely defined within iommu/iommu.c and
	 * not public available. iommu/iommu.c is not used by our port, so adding
	 * a dummy valid pointer is sufficient to get i915_vtd_active working.
	 */
	i915->drm.dev->iommu_group = kzalloc(4096, 0);
	if (!i915_vtd_active(i915))
		printk("i915_vtd_active is off, which may cause random runtime"
		       "IOMMU faults on kernels with enabled IOMMUs\n");

	return 0;
}


int intel_gt_probe_all(struct drm_i915_private * i915)
{
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);
	struct intel_gt *gt = &i915->gt0;
	phys_addr_t phys_addr;
	unsigned int mmio_bar;
	int ret;

	mmio_bar = intel_mmio_bar(GRAPHICS_VER(i915));
	phys_addr = pci_resource_start(pdev, mmio_bar);

	/*
	 * We always have at least one primary GT on any device
	 * and it has been already initialized early during probe
	 * in i915_driver_probe()
	 */
	gt->i915 = i915;
	gt->name = "Primary GT";
	gt->info.engine_mask = INTEL_INFO(i915)->platform_engine_mask;

	/*  intel_gt_tile_setup() emulation - start */
	intel_uncore_init_early(gt->uncore, gt);

	ret = intel_uncore_setup_mmio(gt->uncore, phys_addr);
	if (ret)
		return ret;

	gt->phys_addr = phys_addr;
	/*  intel_gt_tile_setup() emulation - end */

	i915->gt[0] = gt;

	return 0;
}


int intel_gt_assign_ggtt(struct intel_gt * gt)
{
	gt->ggtt = drmm_kzalloc(&gt->i915->drm, sizeof(*gt->ggtt), GFP_KERNEL);

	return gt->ggtt ? 0 : -ENOMEM;
}


enum i915_map_type intel_gt_coherent_map_type(struct intel_gt *gt,
					      struct drm_i915_gem_object *obj,
					      bool always_coherent)
{
	/*
	 * Wa_22016122933: always return I915_MAP_WC for Media
	 * version 13.0 when the object is on the Media GT
	 */
	if (i915_gem_object_is_lmem(obj) || intel_gt_needs_wa_22016122933(gt))
		return I915_MAP_WC;
	if (HAS_LLC(gt->i915) || always_coherent)
		return I915_MAP_WB;
	else
		return I915_MAP_WC;
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


unsigned long __FIXADDR_TOP = 0xfffff000;


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
 * see linux/src/linux/mm/page_alloc.c - mostly original code, beside __folio_put
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
 * see linux/src/linux/mm/swap.c - this is a very shorten version of it
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
 * see linux/src/linux/mm/swap.c - original code
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
 * see linux/src/linux/mm/swap.c - this is a very shorten version of it
 */
void release_pages(release_pages_arg arg, int nr)
{
	int i;
	struct encoded_page **encoded = arg.encoded_pages;

	for (i = 0; i < nr; i++) {
		struct folio *folio;

		/* Turn any of the argument types into a folio */
		folio = page_folio(encoded_page_ptr(encoded[i]));

		if (is_huge_zero_page(&folio->page))
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
