/*
 * \brief  Linux emulation C helper functions
 * \author Stefan Kalkowski
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <lx_emul_c.h>
#include <../drivers/gpu/drm/i915/i915_drv.h>
#include <../drivers/gpu/drm/i915/intel_drv.h>
#include <drm/drm_atomic_helper.h>

extern struct drm_framebuffer *
lx_c_intel_framebuffer_create(struct drm_device *dev,
                         struct drm_mode_fb_cmd2 *mode_cmd,
                         struct drm_i915_gem_object *obj);


int intel_sanitize_enable_execlists(struct drm_device *dev,
                                    int enable_execlists)
{
	if (INTEL_INFO(dev)->gen >= 9)
		return 1;
	return 0;
}


void lx_c_allocate_framebuffer(struct drm_device * dev,
                                struct lx_c_fb_config *c)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_mode_fb_cmd2 * r;
	struct drm_i915_gem_object * obj = NULL;

	mutex_lock(&dev->struct_mutex);

	/* for linear buffers the pitch needs to be 64 byte aligned */
	c->pitch = roundup(c->width * c->bpp, 64);
	c->size  = roundup(c->pitch * c->height, PAGE_SIZE);
	if (c->size * 2 < dev_priv->gtt.stolen_usable_size)
		obj = i915_gem_object_create_stolen(dev, c->size);
	if (obj == NULL)
		obj = i915_gem_alloc_object(dev, c->size);
	if (obj == NULL) goto out2;

	r = (struct drm_mode_fb_cmd2*) kzalloc(sizeof(struct drm_mode_fb_cmd2), 0);
	if (!r) goto err2;
	r->width        = c->width;
	r->height       = c->height;
	r->pixel_format = DRM_FORMAT_RGB565;
	r->pitches[0]   = c->pitch;
	c->lx_fb = lx_c_intel_framebuffer_create(dev, r, obj);
	if (IS_ERR(c->lx_fb)) goto err2;

	if (intel_pin_and_fence_fb_obj(NULL, c->lx_fb, NULL, NULL, NULL))
		goto err1;

	c->addr = ioremap_wc(dev_priv->gtt.mappable_base
	                     + i915_gem_obj_ggtt_offset(obj), c->size);

	memset_io(c->addr, 0, c->size);
	goto out1;

err1:
	drm_framebuffer_remove(c->lx_fb);
err2:
	c->lx_fb = NULL;
	drm_gem_object_unreference(&obj->base);
out1:
	kfree(r);
out2:
	mutex_unlock(&dev->struct_mutex);
}


void lx_c_set_mode(struct drm_device * dev, struct drm_connector * connector,
                    struct drm_framebuffer *fb, struct drm_display_mode *mode)
{
	struct drm_crtc *crtc = NULL;
	struct drm_encoder *encoder = connector->encoder;

	if (!mode) return;

	if (!encoder) {
		struct drm_encoder *enc;
		list_for_each_entry(enc, &dev->mode_config.encoder_list, head) {
			unsigned i;
			for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++)
				if (connector->encoder_ids[i] == enc->base.id) break;

			if (i == DRM_CONNECTOR_MAX_ENCODER) continue;

			bool used = false;
			struct drm_connector *c;
			list_for_each_entry(c, &dev->mode_config.connector_list, head) {
				if (c->encoder == enc) used = true;
			}
			if (used) continue;
			encoder = enc;
			break;
		}
	}

	if (!encoder) {
		DRM_DEBUG("Found no encoder for the connector %s\n", connector->name);
		return;
	}

	crtc = encoder->crtc;
	if (!crtc) {
		unsigned i = 0;
		struct drm_crtc *c;
		list_for_each_entry(c, &dev->mode_config.crtc_list, head) {
			if (!(encoder->possible_crtcs & (1 << i))) continue;
			if (c->state->enable) continue;
			crtc = c;
			break;
		}
	}

	if (!crtc) {
		DRM_DEBUG("Found no crtc for the connector %s\n", connector->name);
		return;
	}

	DRM_DEBUG("set mode %s for connector %s\n", mode->name, connector->name);

	struct drm_mode_set set;
	set.crtc = crtc;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.connectors = &connector;
	set.num_connectors = 1;
	set.fb = fb;
	drm_atomic_helper_set_config(&set);
}


void lx_c_set_driver(struct drm_device * dev, void * driver)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	ASSERT(!dev_priv->audio_component);
	dev_priv->audio_component = (struct i915_audio_component *) driver;
}


void* lx_c_get_driver(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	return (void*) dev_priv->audio_component;
}
