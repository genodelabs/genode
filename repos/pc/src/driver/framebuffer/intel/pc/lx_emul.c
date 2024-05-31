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
#include <lx_emul/page_virt.h>
#include <lx_emul/shmem_file.h>

#include <linux/dma-fence.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include "i915_drv.h"
#include "intel_pci_config.h"
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


extern int intel_root_gt_init_early(struct drm_i915_private * i915);
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

	/* disable panel self refresh (required for FUJITSU S937/S938) */
	i915->params.enable_psr = 0;

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


extern int intel_gt_probe_all(struct drm_i915_private * i915);
int intel_gt_probe_all(struct drm_i915_private * i915)
{
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);
	struct intel_gt *gt = &i915->gt0;
	phys_addr_t phys_addr;
	unsigned int mmio_bar;
	int ret;

	mmio_bar = GRAPHICS_VER(i915) == 2 ? GEN2_GTTMMADR_BAR : GTTMMADR_BAR;
	phys_addr = pci_resource_start(pdev, mmio_bar);

	/*
	 * We always have at least one primary GT on any device
	 * and it has been already initialized early during probe
	 * in i915_driver_probe()
	 */
	gt->i915 = i915;
	gt->name = "Primary GT";
	gt->info.engine_mask = RUNTIME_INFO(i915)->platform_engine_mask;

	intel_uncore_init_early(gt->uncore, gt);

	ret = intel_uncore_setup_mmio(gt->uncore, phys_addr);
	if (ret)
		return ret;

	gt->phys_addr = phys_addr;

	i915->gt[0] = gt;

	return 0;
}


extern int intel_gt_assign_ggtt(struct intel_gt * gt);
int intel_gt_assign_ggtt(struct intel_gt * gt)
{
	gt->ggtt = drmm_kzalloc(&gt->i915->drm, sizeof(*gt->ggtt), GFP_KERNEL);

	return gt->ggtt ? 0 : -ENOMEM;
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

	//struct intel_device_info *info = mkwrite_device_info(dev_priv);
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
