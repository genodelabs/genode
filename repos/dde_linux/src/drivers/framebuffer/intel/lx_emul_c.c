#include "lx_emul_private.h"
#include <../drivers/gpu/drm/i915/i915_drv.h>
#include <../drivers/gpu/drm/i915/intel_drv.h>
#include <drm/drm_atomic_helper.h>

extern struct drm_framebuffer *
dde_c_intel_framebuffer_create(struct drm_device *dev,
                         struct drm_mode_fb_cmd2 *mode_cmd,
                         struct drm_i915_gem_object *obj);


int intel_sanitize_enable_execlists(struct drm_device *dev,
                                    int enable_execlists)
{
	if (INTEL_INFO(dev)->gen >= 9)
		return 1;
	return 0;
}


struct drm_framebuffer*
dde_c_allocate_framebuffer(int width, int height, void ** base,
                           uint64_t * size, struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_framebuffer * fb = NULL;
	struct drm_mode_fb_cmd2 * r;
	struct drm_i915_gem_object * obj = NULL;

	mutex_lock(&dev->struct_mutex);

	*size = roundup(width * height * 2, PAGE_SIZE);
	if (*size * 2 < dev_priv->gtt.stolen_usable_size)
		obj = i915_gem_object_create_stolen(dev, *size);
	if (obj == NULL)
		obj = i915_gem_alloc_object(dev, *size);
	if (obj == NULL) goto out2;

	r = (struct drm_mode_fb_cmd2*) kzalloc(sizeof(struct drm_mode_fb_cmd2), 0);
	if (!r) goto err2;
	r->width        = width;
	r->height       = height;
	r->pixel_format = DRM_FORMAT_RGB565;
	r->pitches[0]   = width * 2;
	fb = dde_c_intel_framebuffer_create(dev, r, obj);
	if (!fb) goto err2;

	if (intel_pin_and_fence_fb_obj(NULL, fb, NULL, NULL, NULL))
		goto err1;

	*base = ioremap_wc(dev_priv->gtt.mappable_base
	                   + i915_gem_obj_ggtt_offset(obj), *size);

	memset_io(*base, 0, *size);
	goto out1;

err1:
	drm_framebuffer_remove(fb);
	fb = NULL;
err2:
	drm_gem_object_unreference(&obj->base);
out1:
	kfree(r);
out2:
	mutex_unlock(&dev->struct_mutex);
	return fb;
}


void dde_c_set_mode(struct drm_device * dev, struct drm_connector * connector,
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


void dde_c_set_driver(struct drm_device * dev, void * driver)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	ASSERT(!dev_priv->audio_component);
	dev_priv->audio_component = (struct i915_audio_component *) driver;
}


void* dde_c_get_driver(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	return (void*) dev_priv->audio_component;
}
