/*
 * \brief  Linux emulation C helper functions
 * \author Stefan Kalkowski
 * \author Christian Prochaska
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


/* Genode includes */
#include <lx_emul.h>
#include <lx_emul_c.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_modeset_helper.h>
#include <imx/imx-drm.h>

void lx_c_allocate_framebuffer(struct drm_device * dev,
                               struct lx_c_fb_config *c)
{
	/* from drm_fbdev_cma_create() */

	struct drm_gem_cma_object *obj;

	c->pitch = c->width * c->bpp;
	c->size  = c->pitch * c->height;

	obj = drm_gem_cma_create(dev, c->size);

	if (obj == NULL)
		return;

	c->addr = obj->vaddr;

	/* from drm_gem_fb_alloc() */

	struct drm_framebuffer *fb = kzalloc(sizeof(*fb), GFP_KERNEL);

	if (fb == NULL)
		goto err;

	struct drm_mode_fb_cmd2 mode_cmd = { 0 };

	mode_cmd.width = c->width;
	mode_cmd.height = c->height;
	mode_cmd.pitches[0] = c->pitch;
	mode_cmd.pixel_format = DRM_FORMAT_XRGB8888;

	drm_helper_mode_fill_fb_struct(dev, fb, &mode_cmd);

	fb->obj[0] = &obj->base;

	static const struct drm_framebuffer_funcs drm_fb_cma_funcs = {
		.destroy = drm_gem_fb_destroy,
	};

	if (drm_framebuffer_init(dev, fb, &drm_fb_cma_funcs) != 0) {
		kfree(fb);
		goto err;
	}

	c->lx_fb = fb;

	memset_io(c->addr, 0, c->size);

	return;

err:
	drm_gem_object_put_unlocked(&obj->base); /* as in drm_gem_cma_create() */
	return;
}


void lx_c_set_mode(struct drm_device * dev, struct drm_connector * connector,
                   struct drm_framebuffer *fb, struct drm_display_mode *mode)
{
	struct drm_crtc        * crtc    = NULL;
	struct drm_encoder     * encoder = connector->encoder;

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
		lx_printf("Found no encoder for the connector %s\n", connector->name);
		return;
	}

	unsigned used_crtc = 0;

	crtc = encoder->crtc;
	if (!crtc) {
		unsigned i = 0;
		struct drm_crtc *c;
		list_for_each_entry(c, &dev->mode_config.crtc_list, head) {
			if (!(encoder->possible_crtcs & (1 << i))) continue;
			if (c->state->enable) {
				used_crtc ++;
				continue;
			}
			crtc = c;
			break;
		}
	}

	if (!crtc) {
		if (mode)
			lx_printf("Found no crtc for the connector %s used/max %u+1/%u\n",
			          connector->name, used_crtc, dev->mode_config.num_crtc);
		return;
	}

	DRM_DEBUG("%s%s for connector %s\n", mode ? "set mode " : "no mode",
	          mode ? mode->name : "", connector->name);

	struct drm_mode_set set;
	set.crtc = crtc;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.connectors = &connector;
	set.num_connectors = mode ? 1 : 0;
	set.fb = mode ? fb : 0;

	uint32_t const ref_cnt_before = drm_framebuffer_read_refcount(fb);
	int ret = drm_atomic_helper_set_config(&set, dev->mode_config.acquire_ctx);
	if (ret)
		lx_printf("Error: set config failed ret=%d refcnt before=%u after=%u\n",
		          ret, ref_cnt_before, drm_framebuffer_read_refcount(fb));
}

void lx_c_set_driver(struct drm_device * dev, void * driver)
{
	struct imx_drm_device *dev_priv = dev->dev_private;
	ASSERT(!dev_priv->fbhelper);
	dev_priv->fbhelper = (struct drm_fbdev_cma *) driver;
}


void* lx_c_get_driver(struct drm_device * dev)
{
	struct imx_drm_device *dev_priv = dev->dev_private;
	return (void*) dev_priv->fbhelper;
}
