/*
 * \brief  Shadow of drivers/gpu/drm/i915/i915_gem.c
 * \author Alexander Boettcher
 * \date   2022-02-03
 */

/*
 * Copyright © 2008-2015 Intel Corporation
 *
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include "i915_drv.h"

#include "intel_pm.h"


int i915_gem_object_unbind(struct drm_i915_gem_object *obj,
			   unsigned long flags)
{
	struct intel_runtime_pm *rpm = &to_i915(obj->base.dev)->runtime_pm;
	LIST_HEAD(still_in_list);
	intel_wakeref_t wakeref;
	struct i915_vma *vma;
	int ret;

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
		struct i915_address_space *vm = vma->vm;

		list_move_tail(&vma->obj_link, &still_in_list);
		if (!i915_vma_is_bound(vma, I915_VMA_BIND_MASK))
			continue;

		if (flags & I915_GEM_OBJECT_UNBIND_TEST) {
			ret = -EBUSY;
			break;
		}

		ret = -EAGAIN;
		if (!i915_vm_tryopen(vm))
			break;

		/* Prevent vma being freed by i915_vma_parked as we unbind */
		vma = __i915_vma_get(vma);
		spin_unlock(&obj->vma.lock);

		if (vma) {
			ret = -EBUSY;
			if (flags & I915_GEM_OBJECT_UNBIND_ACTIVE ||
			    !i915_vma_is_active(vma)) {
				if (flags & I915_GEM_OBJECT_UNBIND_VM_TRYLOCK) {
					if (mutex_trylock(&vma->vm->mutex)) {
						ret = __i915_vma_unbind(vma);
						mutex_unlock(&vma->vm->mutex);
					} else {
						ret = -EBUSY;
					}
				} else {
					ret = i915_vma_unbind(vma);
				}
			}

			__i915_vma_put(vma);
		}

		i915_vm_close(vm);
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
			    const struct i915_ggtt_view *view,
			    u64 size, u64 alignment, u64 flags)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_ggtt *ggtt = &i915->ggtt;
	struct i915_vma *vma;
	int ret;

	if (flags & PIN_MAPPABLE &&
	    (!view || view->type == I915_GGTT_VIEW_NORMAL)) {
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

			if (flags & PIN_MAPPABLE &&
			    vma->fence_size > ggtt->mappable_end / 2)
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

	if (ww)
		ret = i915_vma_pin_ww(vma, ww, size, alignment, flags | PIN_GLOBAL);
	else
		ret = i915_vma_pin(vma, size, alignment, flags | PIN_GLOBAL);

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

void i915_gem_ww_ctx_init(struct i915_gem_ww_ctx *ww, bool intr)
{
	ww_acquire_init(&ww->ctx, &reservation_ww_class);
	INIT_LIST_HEAD(&ww->obj_list);
	ww->intr = intr;
	ww->contended = NULL;
}

static void i915_gem_ww_ctx_unlock_all(struct i915_gem_ww_ctx *ww)
{
	struct drm_i915_gem_object *obj;

	while ((obj = list_first_entry_or_null(&ww->obj_list, struct drm_i915_gem_object, obj_link))) {
		list_del(&obj->obj_link);
		i915_gem_object_unlock(obj);
	}
}

void i915_gem_ww_ctx_fini(struct i915_gem_ww_ctx *ww)
{
	i915_gem_ww_ctx_unlock_all(ww);
	WARN_ON(ww->contended);
	ww_acquire_fini(&ww->ctx);
}

static void i915_gem_init__mm(struct drm_i915_private *i915)
{
	spin_lock_init(&i915->mm.obj_lock);

	init_llist_head(&i915->mm.free_list);

	INIT_LIST_HEAD(&i915->mm.purge_list);
	INIT_LIST_HEAD(&i915->mm.shrink_list);

	i915_gem_init__objects(i915);
}


int i915_gem_init(struct drm_i915_private *dev_priv)
{
	int ret;

	/* We need to fallback to 4K pages if host doesn't support huge gtt. */
/*
	if (intel_vgpu_active(dev_priv) && !intel_vgpu_has_huge_gtt(dev_priv))
		mkwrite_device_info(dev_priv)->page_sizes =
			I915_GTT_PAGE_SIZE_4K;
*/

	ret = i915_gem_init_userptr(dev_priv);
	if (ret)
		return ret;

/*
	intel_uc_fetch_firmwares(&dev_priv->gt.uc);
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
	ret = intel_gt_init(&dev_priv->gt);
	if (ret)
		goto err_unlock;
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

/*
	if (ret != -EIO)
		intel_uc_cleanup_firmwares(&dev_priv->gt.uc);
*/

	if (ret == -EIO) {
		/*
		 * Allow engines or uC initialisation to fail by marking the GPU
		 * as wedged. But we only want to do this when the GPU is angry,
		 * for all other failure, such as an allocation failure, bail.
		 */
/*
		if (!intel_gt_is_wedged(&dev_priv->gt)) {
			i915_probe_error(dev_priv,
					 "Failed to initialize GPU, declaring it wedged!\n");
			intel_gt_set_wedged(&dev_priv->gt);
		}
*/
		/* Minimal basic recovery for KMS */
		ret = i915_ggtt_enable_hw(dev_priv);
		i915_ggtt_resume(&dev_priv->ggtt);

		intel_init_clock_gating(dev_priv);
	}

	i915_gem_drain_freed_objects(dev_priv);

	return ret;
}

void i915_gem_driver_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void i915_gem_init_early(struct drm_i915_private *dev_priv)
{
	/* 4 * 4M XXX */
	unsigned const ram_pages = 4 * 1024;

	i915_gem_init__mm(dev_priv);
/*
	i915_gem_init__contexts(dev_priv);
*/

	lx_emul_trace(__func__);

	spin_lock_init(&dev_priv->fb_tracking.lock);

	totalram_pages_add(ram_pages);
}


int i915_gem_open(struct drm_i915_private *i915, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv;

	DRM_DEBUG("\n");

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;
	file_priv->dev_priv = i915;
	file_priv->file = file;

	file_priv->bsd_engine = -1;
	file_priv->hang_timestamp = jiffies;

/*
	ret = i915_gem_context_open(i915, file);
	if (ret)
		kfree(file_priv);

	return ret;
*/
	return 0;
}


int i915_gem_init_userptr(struct drm_i915_private *dev_priv)
{
#ifdef CONFIG_MMU_NOTIFIER
	spin_lock_init(&dev_priv->mm.notifier_lock);
#endif

	return 0;
}
