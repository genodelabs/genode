/*
 * \brief  Linux emulation environment specific to this driver
 *         Shadow of intel_gt.c with mostly original code
 * \author Alexander Boettcher
 * \date   2025-07-15
 */

/*
 * Copyright (C) 2021-2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * SPDX-License-Identifier: MIT
 * Copyright © 2019 Intel Corporation
 */

#include <drm/drm_managed.h>
#include <drm/intel/intel-gtt.h>

#include "i915_drv.h"
#include "intel_pci_config.h"
#include "i915/gem/i915_gem_lmem.h"
#include "i915/gt/intel_gt.h"

void intel_gt_common_init_early(struct intel_gt *gt)
{
	spin_lock_init(gt->irq_lock);

	INIT_LIST_HEAD(&gt->closed_vma);
	spin_lock_init(&gt->closed_lock);

	init_llist_head(&gt->watchdog.list);
#if 0
	INIT_WORK(&gt->watchdog.work, intel_gt_watchdog_work);
#endif

#if 0
	intel_gt_init_buffer_pool(gt);
	intel_gt_init_reset(gt);
	intel_gt_init_requests(gt);
	intel_gt_init_timelines(gt);
	intel_gt_init_tlb(gt);
	intel_gt_pm_init_early(gt);
#endif

	intel_wopcm_init_early(&gt->wopcm);
	intel_uc_init_early(&gt->uc);
#if 0
	intel_rps_init_early(&gt->rps);
#endif
}

/* Preliminary initialization of Tile 0 */
int intel_root_gt_init_early(struct drm_i915_private *i915)
{
	struct intel_gt *gt;

	gt = drmm_kzalloc(&i915->drm, sizeof(*gt), GFP_KERNEL);
	if (!gt)
		return -ENOMEM;

	i915->gt[0] = gt;

	gt->i915 = i915;
	gt->uncore = &i915->uncore;
	gt->irq_lock = drmm_kzalloc(&i915->drm, sizeof(*gt->irq_lock), GFP_KERNEL);
	if (!gt->irq_lock)
		return -ENOMEM;

	intel_gt_common_init_early(gt);

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

int intel_gt_assign_ggtt(struct intel_gt *gt)
{
	/* Media GT shares primary GT's GGTT */
	if (gt->type == GT_MEDIA) {
		gt->ggtt = to_gt(gt->i915)->ggtt;
	} else {
		gt->ggtt = i915_ggtt_create(gt->i915);
		if (IS_ERR(gt->ggtt))
			return PTR_ERR(gt->ggtt);
	}

	list_add_tail(&gt->ggtt_link, &gt->ggtt->gt_list);

	return 0;
}

static int intel_gt_tile_setup(struct intel_gt *gt, phys_addr_t phys_addr)
{
	int ret;

	if (!gt_is_root(gt)) {
		struct intel_uncore *uncore;
		spinlock_t *irq_lock;

		uncore = drmm_kzalloc(&gt->i915->drm, sizeof(*uncore), GFP_KERNEL);
		if (!uncore)
			return -ENOMEM;

		irq_lock = drmm_kzalloc(&gt->i915->drm, sizeof(*irq_lock), GFP_KERNEL);
		if (!irq_lock)
			return -ENOMEM;

		gt->uncore = uncore;
		gt->irq_lock = irq_lock;

		intel_gt_common_init_early(gt);
	}

	intel_uncore_init_early(gt->uncore, gt);

	ret = intel_uncore_setup_mmio(gt->uncore, phys_addr);
	if (ret)
		return ret;

	gt->phys_addr = phys_addr;

	return 0;
}

int intel_gt_probe_all(struct drm_i915_private *i915)
{
	struct pci_dev *pdev = to_pci_dev(i915->drm.dev);
	struct intel_gt *gt = to_gt(i915);
	const struct intel_gt_definition *gtdef;
	phys_addr_t phys_addr;
	unsigned int mmio_bar;
	unsigned int i;
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

#if 0
	gt_dbg(gt, "Setting up %s\n", gt->name);
#endif
	ret = intel_gt_tile_setup(gt, phys_addr);
	if (ret)
		return ret;

	if (!HAS_EXTRA_GT_LIST(i915))
		return 0;

	(void)i;
	/* skip all other gtdef = &INTEL_INFO(i915)->extra_gt_list[i - 1]; */

	return 0;

err:
	i915_probe_error(i915, "Failed to initialize %s! (%d)\n", gtdef->name, ret);
	return ret;
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

bool intel_gt_needs_wa_22016122933(struct intel_gt *gt)
{
	return MEDIA_VER_FULL(gt->i915) == IP_VER(13, 0) && gt->type == GT_MEDIA;
}


/**
 * intel_gt_is_bind_context_ready - Check if context binding is ready
 *
 * @gt: GT structure
 *
 * This function returns binder context's ready status.
 */
bool intel_gt_is_bind_context_ready(struct intel_gt *gt)
{
#if 0
	struct intel_engine_cs *engine = gt->engine[BCS0];

	if (engine)
		return engine->bind_context_ready;
#endif

	return false;
}
