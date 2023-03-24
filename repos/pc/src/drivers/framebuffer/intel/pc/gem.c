/*
 * \brief  Shadow of drivers/gpu/drm/i915/i915_gem.c
 * \author Alexander Boettcher
 * \date   2022-02-03
 */

/*
 * Copyright © 2008-2023 Intel Corporation
 *
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include "i915_drv.h"

#include "intel_pm.h"
#include "../drivers/gpu/drm/i915/gt/intel_gt.h"
#include "../drivers/gpu/drm/i915/i915_file_private.h"


int i915_gem_object_unbind(struct drm_i915_gem_object *obj,
			   unsigned long flags)
{
	struct intel_runtime_pm *rpm = &to_i915(obj->base.dev)->runtime_pm;
	bool vm_trylock = !!(flags & I915_GEM_OBJECT_UNBIND_VM_TRYLOCK);
	LIST_HEAD(still_in_list);
	intel_wakeref_t wakeref;
	struct i915_vma *vma;
	int ret;

	assert_object_held(obj);

	if (list_empty(&obj->vma.list))
		return 0;

	/*
	 * As some machines use ACPI to handle runtime-resume callbacks, and
	 * ACPI is quite kmalloc happy, we cannot resume beneath the vm->mutex
	 * as they are required by the shrinker. Ergo, we wake the device up
	 * first just in case.
	 */
	wakeref = intel_runtime_pm_get(rpm);

try_again:
	ret = 0;
	spin_lock(&obj->vma.lock);
	while (!ret && (vma = list_first_entry_or_null(&obj->vma.list,
						       struct i915_vma,
						       obj_link))) {
		list_move_tail(&vma->obj_link, &still_in_list);
		if (!i915_vma_is_bound(vma, I915_VMA_BIND_MASK))
			continue;

		if (flags & I915_GEM_OBJECT_UNBIND_TEST) {
			ret = -EBUSY;
			break;
		}

		/*
		 * Requiring the vm destructor to take the object lock
		 * before destroying a vma would help us eliminate the
		 * i915_vm_tryget() here, AND thus also the barrier stuff
		 * at the end. That's an easy fix, but sleeping locks in
		 * a kthread should generally be avoided.
		 */
		ret = -EAGAIN;
		if (!i915_vm_tryget(vma->vm))
			break;

		spin_unlock(&obj->vma.lock);

		/*
		 * Since i915_vma_parked() takes the object lock
		 * before vma destruction, it won't race us here,
		 * and destroy the vma from under us.
		 */

		ret = -EBUSY;
		if (flags & I915_GEM_OBJECT_UNBIND_ASYNC) {
			assert_object_held(vma->obj);
			ret = i915_vma_unbind_async(vma, vm_trylock);
		}

		if (ret == -EBUSY && (flags & I915_GEM_OBJECT_UNBIND_ACTIVE ||
				      !i915_vma_is_active(vma))) {
			if (vm_trylock) {
				if (mutex_trylock(&vma->vm->mutex)) {
					ret = __i915_vma_unbind(vma);
					mutex_unlock(&vma->vm->mutex);
				}
			} else {
				ret = i915_vma_unbind(vma);
			}
		}

		i915_vm_put(vma->vm);
		spin_lock(&obj->vma.lock);
	}
	list_splice_init(&still_in_list, &obj->vma.list);
	spin_unlock(&obj->vma.lock);

	if (ret == -EAGAIN && flags & I915_GEM_OBJECT_UNBIND_BARRIER) {
		rcu_barrier(); /* flush the i915_vm_release() */
		goto try_again;
	}

	intel_runtime_pm_put(rpm, wakeref);

	return ret;
}

static void discard_ggtt_vma(struct i915_vma *vma)
{
	struct drm_i915_gem_object *obj = vma->obj;

	spin_lock(&obj->vma.lock);
	if (!RB_EMPTY_NODE(&vma->obj_node)) {
		rb_erase(&vma->obj_node, &obj->vma.tree);
		RB_CLEAR_NODE(&vma->obj_node);
	}
	spin_unlock(&obj->vma.lock);
}

struct i915_vma *
i915_gem_object_ggtt_pin_ww(struct drm_i915_gem_object *obj,
			    struct i915_gem_ww_ctx *ww,
			    const struct i915_gtt_view *view,
			    u64 size, u64 alignment, u64 flags)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;
	struct i915_vma *vma;
	int ret;

	GEM_WARN_ON(!ww);

	if (flags & PIN_MAPPABLE &&
	    (!view || view->type == I915_GTT_VIEW_NORMAL)) {
		/*
		 * If the required space is larger than the available
		 * aperture, we will not able to find a slot for the
		 * object and unbinding the object now will be in
		 * vain. Worse, doing so may cause us to ping-pong
		 * the object in and out of the Global GTT and
		 * waste a lot of cycles under the mutex.
		 */
		if (obj->base.size > ggtt->mappable_end)
			return ERR_PTR(-E2BIG);

		/*
		 * If NONBLOCK is set the caller is optimistically
		 * trying to cache the full object within the mappable
		 * aperture, and *must* have a fallback in place for
		 * situations where we cannot bind the object. We
		 * can be a little more lax here and use the fallback
		 * more often to avoid costly migrations of ourselves
		 * and other objects within the aperture.
		 *
		 * Half-the-aperture is used as a simple heuristic.
		 * More interesting would to do search for a free
		 * block prior to making the commitment to unbind.
		 * That caters for the self-harm case, and with a
		 * little more heuristics (e.g. NOFAULT, NOEVICT)
		 * we could try to minimise harm to others.
		 */
		if (flags & PIN_NONBLOCK &&
		    obj->base.size > ggtt->mappable_end / 2)
			return ERR_PTR(-ENOSPC);
	}

new_vma:
	vma = i915_vma_instance(obj, &ggtt->vm, view);
	if (IS_ERR(vma))
		return vma;

	if (i915_vma_misplaced(vma, size, alignment, flags)) {
		if (flags & PIN_NONBLOCK) {
			if (i915_vma_is_pinned(vma) || i915_vma_is_active(vma))
				return ERR_PTR(-ENOSPC);

			/*
			 * If this misplaced vma is too big (i.e, at-least
			 * half the size of aperture) or hasn't been pinned
			 * mappable before, we ignore the misplacement when
			 * PIN_NONBLOCK is set in order to avoid the ping-pong
			 * issue described above. In other words, we try to
			 * avoid the costly operation of unbinding this vma
			 * from the GGTT and rebinding it back because there
			 * may not be enough space for this vma in the aperture.
			 */
			if (flags & PIN_MAPPABLE &&
			    (vma->fence_size > ggtt->mappable_end / 2 ||
			    !i915_vma_is_map_and_fenceable(vma)))
				return ERR_PTR(-ENOSPC);
		}

		if (i915_vma_is_pinned(vma) || i915_vma_is_active(vma)) {
			discard_ggtt_vma(vma);
			goto new_vma;
		}

		ret = i915_vma_unbind(vma);
		if (ret)
			return ERR_PTR(ret);
	}

	ret = i915_vma_pin_ww(vma, ww, size, alignment, flags | PIN_GLOBAL);

	if (ret)
		return ERR_PTR(ret);

	if (vma->fence && !i915_gem_object_is_tiled(obj)) {
		mutex_lock(&ggtt->vm.mutex);
		i915_vma_revoke_fence(vma);
		mutex_unlock(&ggtt->vm.mutex);
	}

	ret = i915_vma_wait_for_bind(vma);
	if (ret) {
		i915_vma_unpin(vma);
		return ERR_PTR(ret);
	}

	return vma;
}

static inline int i915_gem_init_userptr(struct drm_i915_private *dev_priv)
{
#ifdef CONFIG_MMU_NOTIFIER
	rwlock_init(&dev_priv->mm.notifier_lock);
#endif

	return 0;
}

int i915_gem_init(struct drm_i915_private *dev_priv)
{
	struct intel_gt *gt;
	unsigned int i;
	int ret;

	/* We need to fallback to 4K pages if host doesn't support huge gtt. */
/*
	if (intel_vgpu_active(dev_priv) && !intel_vgpu_has_huge_gtt(dev_priv))
		RUNTIME_INFO(dev_priv)->page_sizes = I915_GTT_PAGE_SIZE_4K;
*/

	ret = i915_gem_init_userptr(dev_priv);
	if (ret)
		return ret;

/*
	intel_uc_fetch_firmwares(&to_gt(dev_priv)->uc);
	intel_wopcm_init(&dev_priv->wopcm);
*/
	ret = i915_init_ggtt(dev_priv);
	if (ret) {
		GEM_BUG_ON(ret == -EIO);
		goto err_unlock;
	}

	/*
	 * Despite its name intel_init_clock_gating applies both display
	 * clock gating workarounds; GT mmio workarounds and the occasional
	 * GT power context workaround. Worse, sometimes it includes a context
	 * register workaround which we need to apply before we record the
	 * default HW state for all contexts.
	 *
	 * FIXME: break up the workarounds and apply them at the right time!
	 */

	intel_init_clock_gating(dev_priv);

/*
	for_each_gt(gt, dev_priv, i) {
		ret = intel_gt_init(gt);
		if (ret)
			goto err_unlock;
	}
*/

	return 0;

	/*
	 * Unwinding is complicated by that we want to handle -EIO to mean
	 * disable GPU submission but keep KMS alive. We want to mark the
	 * HW as irrevisibly wedged, but keep enough state around that the
	 * driver doesn't explode during runtime.
	 */
err_unlock:
	i915_gem_drain_workqueue(dev_priv);

	if (ret != -EIO) {
		for_each_gt(gt, dev_priv, i) {
			intel_gt_driver_remove(gt);
			intel_gt_driver_release(gt);
			/*
			intel_uc_cleanup_firmwares(&gt->uc);
			*/
		}
	}

	if (ret == -EIO) {
		/*
		 * Allow engines or uC initialisation to fail by marking the GPU
		 * as wedged. But we only want to do this when the GPU is angry,
		 * for all other failure, such as an allocation failure, bail.
		 */
		/*
		for_each_gt(gt, dev_priv, i) {
			if (!intel_gt_is_wedged(gt)) {
				i915_probe_error(dev_priv,
						 "Failed to initialize GPU, declaring it wedged!\n");
				intel_gt_set_wedged(gt);
			}
		}
		*/

		/* Minimal basic recovery for KMS */
		ret = i915_ggtt_enable_hw(dev_priv);
		i915_ggtt_resume(to_gt(dev_priv)->ggtt);
		intel_init_clock_gating(dev_priv);
	}

	i915_gem_drain_freed_objects(dev_priv);

	return ret;
}

void i915_gem_driver_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}

static void i915_gem_init__mm(struct drm_i915_private *i915)
{
	spin_lock_init(&i915->mm.obj_lock);

	init_llist_head(&i915->mm.free_list);

	INIT_LIST_HEAD(&i915->mm.purge_list);
	INIT_LIST_HEAD(&i915->mm.shrink_list);

	i915_gem_init__objects(i915);
}

void i915_gem_init_early(struct drm_i915_private *dev_priv)
{
	i915_gem_init__mm(dev_priv);
/*
	i915_gem_init__contexts(dev_priv);
*/

	lx_emul_trace(__func__);

	spin_lock_init(&dev_priv->display.fb_tracking.lock);

	/*
	 * Used by resource_size() check in shmem_get_pages in
	 * drivers/gpu/drm/i915/gem/i915_gem_shmem.c and initialized in
	 * i915_gem_shmem_setup() using totalram_pages()
	 *
	 * The memory is managed by Genode, so we have just to provide a
	 * value which is "big" enough truncated by the max available memory.
	 */
	totalram_pages_add(emul_avail_ram() / 4096);
}


int i915_gem_open(struct drm_i915_private *i915, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv;
	struct i915_drm_client *client;
	int ret = -ENOMEM;

	DRM_DEBUG("\n");

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		goto err_alloc;

	client = i915_drm_client_add(&i915->clients);
	if (IS_ERR(client)) {
		ret = PTR_ERR(client);
		goto err_client;
	}

	file->driver_priv = file_priv;
	file_priv->dev_priv = i915;
	file_priv->file = file;
	file_priv->client = client;

	file_priv->bsd_engine = -1;
	file_priv->hang_timestamp = jiffies;

/*
	ret = i915_gem_context_open(i915, file);
	if (ret)
		goto err_context;
*/

	return 0;

	/*
err_context:
	i915_drm_client_put(client);
	*/
err_client:
	kfree(file_priv);
err_alloc:
	return ret;
}
