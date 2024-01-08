/*
 * \brief  Post kernel activity
 * \author Alexander Boettcher
 * \date   2022-03-08
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/fb.h> /* struct fb_info */
#include <linux/sched/task.h>

#include <drm/drm_client.h>
#include <drm_crtc_internal.h>

#include "i915_drv.h"
#include "display/intel_backlight.h"
#include "display/intel_display_types.h"
#include "display/intel_fb_pin.h"

#include "lx_emul.h"


enum { MAX_BRIGHTNESS = 100, INVALID_BRIGHTNESS = MAX_BRIGHTNESS + 1 };


       struct task_struct    * lx_user_task = NULL;
static struct drm_client_dev * dev_client   = NULL;


static int user_register_fb(struct drm_client_dev   const * const dev,
                            struct fb_info                * const info,
                            struct drm_mode_fb_cmd2 const * const dumb_fb);


static int user_attach_fb_to_crtc(struct drm_client_dev          * const dev,
                                  struct drm_connector     const * const connector,
                                  struct drm_crtc          const * const crtc,
                                  struct drm_mode_modeinfo const * const mode,
                                  unsigned                         const fb_id,
                                  bool                             const enable);

static int check_resize_fb(struct drm_client_dev       * const dev,
                           struct drm_mode_create_dumb * const gem_dumb,
                           struct drm_mode_fb_cmd2     * const dumb_fb,
                           unsigned                      const width,
                           unsigned                      const height);


static inline bool mode_larger(struct drm_display_mode const * const x,
                               struct drm_display_mode const * const y)
{
	return (uint64_t)x->hdisplay * (uint64_t)x->vdisplay >
	       (uint64_t)y->hdisplay * (uint64_t)y->vdisplay;
}


static inline bool conf_smaller_max_mode(struct genode_mode      const * const g,
                                         struct drm_display_mode const * const p)
{
	return (uint64_t)g->max_width * (uint64_t)g->max_height <
	       (uint64_t)p->hdisplay  * (uint64_t)p->vdisplay;
}


static inline bool conf_larger_mode(struct genode_mode      const * const g,
                                    struct drm_display_mode const * const p)
{
	return (uint64_t)g->width    * (uint64_t)g->height >
	       (uint64_t)p->hdisplay * (uint64_t)p->vdisplay;
}


static inline bool fb_smaller_mode(struct fb_info          const * const info,
                                   struct drm_display_mode const * const mode)
{
	return (uint64_t)info->var.xres * (uint64_t)info->var.yres <
	       (uint64_t)mode->vdisplay * (uint64_t)mode->hdisplay;
}


/*
 * Heuristic to calculate max resolution across all connectors
 */
static void preferred_mode(struct drm_device const * const dev,
                           struct drm_display_mode * const prefer,
                           struct drm_display_mode * const min_mode)
{
	struct drm_connector          *connector        = NULL;
	struct drm_display_mode       *mode             = NULL;
	unsigned                       connector_usable = 0;
	struct drm_display_mode        max_enforcement  = { };
	struct drm_connector_list_iter conn_iter;

	/* read Genode's config per connector */
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		struct drm_display_mode smallest  = { .hdisplay = ~0, .vdisplay = ~0 };
		struct genode_mode      conf_mode = { .enabled = 1 };
		unsigned                mode_id   = 0;

		/* check for connector configuration on Genode side */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		if (!conf_mode.enabled)
			continue;

		/* look for smallest possible mode or if a specific mode is forced */
		list_for_each_entry(mode, &connector->modes, head) {
			mode_id ++;

			if (!mode)
				continue;

			if (mode_larger(&smallest, mode)) {
				smallest.hdisplay = mode->hdisplay;
				smallest.vdisplay = mode->vdisplay;
			}

			if (!conf_mode.id)
				continue;

			if (!mode || conf_mode.id != mode_id)
				continue;

			conf_mode.width  = mode->hdisplay;
			conf_mode.height = mode->vdisplay;

			break;
		}

		if (mode_id)
			connector_usable ++;

		/*
		 * If at least on mode is available, store smallest mode if it
		 * is larger than min_mode of other connectors
		 */
		if (mode_id && mode_larger(&smallest, min_mode))
			*min_mode = smallest;

		if (conf_mode.force_width && conf_mode.force_height) {
			/*
			 * Even so the force_* mode is selected, a configured mode for
			 * a connector is considered, effectively the framebuffer content
			 * will be shown smaller in the upper corner of the monitor
			 */
			if (conf_larger_mode(&conf_mode, min_mode)) {
				min_mode->hdisplay = conf_mode.width;
				min_mode->vdisplay = conf_mode.height;
			}

			/* enforce the force mode */
			conf_mode.width  = conf_mode.force_width;
			conf_mode.height = conf_mode.force_height;
		}

		/* maximal resolution enforcement */
		if (conf_mode.max_width && conf_mode.max_height) {
			max_enforcement.hdisplay = conf_mode.max_width;
			max_enforcement.vdisplay = conf_mode.max_height;
			if (conf_smaller_max_mode(&conf_mode, prefer))
				continue;
		}

		if (!conf_mode.width || !conf_mode.height)
			continue;

		if (conf_larger_mode(&conf_mode, prefer)) {
			prefer->hdisplay = conf_mode.width;
			prefer->vdisplay = conf_mode.height;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* no mode due to early bootup or all connectors are disabled by config */
	if (!min_mode->hdisplay || !min_mode->vdisplay)
		return;

	/* we got a preferred resolution */
	if (prefer->hdisplay && prefer->vdisplay)
		return;

	/* if too large or nothing configured by Genode's config */
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		list_for_each_entry(mode, &connector->modes, head) {
			if (!mode)
				continue;

			if (mode_larger(min_mode, mode))
				continue;

			if (max_enforcement.hdisplay && max_enforcement.vdisplay) {
				if (mode_larger(mode, &max_enforcement))
					continue;
			}

			if (mode_larger(mode, prefer)) {
				prefer->hdisplay = mode->hdisplay;
				prefer->vdisplay = mode->vdisplay;
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* handle the "never should happen case" gracefully */
	if (!prefer->hdisplay || !prefer->vdisplay)
		*prefer = *min_mode;
}


static void set_brightness(unsigned brightness, struct drm_connector * connector)
{
	struct intel_connector * intel_c = to_intel_connector(connector);
	if (intel_c)
		intel_backlight_set_acpi(intel_c->base.state, brightness, MAX_BRIGHTNESS);
}


static unsigned get_brightness(struct drm_connector * const connector,
                               unsigned const brightness_error)
{
	struct intel_connector * intel_c = NULL;
	struct intel_panel     * panel   = NULL;
	unsigned ret;

	if (!connector)
		return brightness_error;

	intel_c = to_intel_connector(connector);
	if (!intel_c)
		return brightness_error;

	panel = &intel_c->panel;

	if (!panel || !panel->backlight.device || !panel->backlight.device->ops ||
	    !panel->backlight.device->ops->get_brightness)
		return brightness_error;

	ret = panel->backlight.device->ops->get_brightness(panel->backlight.device);

	/* in percentage */
	return ret * MAX_BRIGHTNESS / panel->backlight.device->props.max_brightness;
}

static struct drm_mode_fb_cmd2     dumb_fb  = {};

static bool reconfigure(struct drm_client_dev * const dev)
{
	static struct drm_mode_create_dumb gem_dumb = {};

	struct drm_display_mode  mode_preferred = {};
	struct drm_display_mode  mode_minimum   = {};
	struct drm_display_mode  framebuffer    = {};
	struct drm_mode_modeinfo user_mode      = {};
	struct drm_display_mode *mode           = NULL;
	struct drm_mode_set     *mode_set       = NULL;
	struct fb_info           report_fb_info = {};
	bool                     report_fb      = false;
	bool                     retry          = false;

	if (!dev || !dev->dev)
		return false;

	preferred_mode(dev->dev, &mode_preferred, &mode_minimum);

	if (!mode_minimum.hdisplay || !mode_minimum.vdisplay) {
		/* no valid modes on any connector on early boot */
		if (!dumb_fb.fb_id)
			return false;

		/* valid connectors but all are disabled by config */
		mode_minimum.hdisplay = dumb_fb.width;
		mode_minimum.vdisplay = dumb_fb.height;
		mode_preferred        = mode_minimum;
	}

	if (mode_larger(&mode_preferred, &mode_minimum))
		framebuffer = mode_preferred;
	else
		framebuffer = mode_minimum;

	{
		int const err = check_resize_fb(dev,
		                                &gem_dumb,
		                                &dumb_fb,
		                                framebuffer.hdisplay,
		                                framebuffer.vdisplay);

		if (err) {
			printk("setting up framebuffer of %ux%u failed - error=%d\n",
			       framebuffer.hdisplay, framebuffer.vdisplay, err);

			return true;
		}
	}

	/* without fb handle created by check_resize_fb we can't proceed */
	if (!dumb_fb.fb_id)
		return retry;

	/* prepare fb info for register_framebuffer() evaluated by Genode side */
	report_fb_info.var.xres         = framebuffer.hdisplay;
	report_fb_info.var.yres         = framebuffer.vdisplay;
	report_fb_info.var.xres_virtual = mode_preferred.hdisplay;
	report_fb_info.var.yres_virtual = mode_preferred.vdisplay;

	drm_client_for_each_modeset(mode_set, dev) {
		struct drm_display_mode *mode_match = NULL;
		unsigned                 mode_id    = 0;
		struct drm_connector    *connector  = NULL;

		struct genode_mode conf_mode = { };

		if (!mode_set->connectors || !*mode_set->connectors)
			continue;

		BUG_ON(!mode_set->crtc);

		/* set connector */
		connector = *mode_set->connectors;

		/* read configuration of connector */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		/* heuristics to find matching mode */
		list_for_each_entry(mode, &connector->modes, head) {
			mode_id ++;

			if (!mode)
				continue;

			/* allocated framebuffer smaller than mode can't be used */
			if (fb_smaller_mode(&report_fb_info, mode))
				continue;

			/* use mode id if configured and matches exactly */
			if (conf_mode.id) {
				if (conf_mode.id != mode_id)
					continue;

				mode_match = mode;
				break;
			}

			/* if invalid, mode is configured in second loop below */
			if (conf_mode.width == 0 || conf_mode.height == 0) {
				break;
			}

			/* no exact match by mode id -> try matching by size */
			if ((mode->hdisplay != conf_mode.width) ||
			    (mode->vdisplay != conf_mode.height))
				continue;

			/* take as default any mode with matching resolution */
			if (!mode_match) {
				mode_match = mode;
				continue;
			}

			/* replace matching mode iif hz matches exactly */
			if ((conf_mode.hz != drm_mode_vrefresh(mode_match)) &&
			    (conf_mode.hz == drm_mode_vrefresh(mode)))
				mode_match = mode;
		}

		/* apply new mode */
		mode_id = 0;
		list_for_each_entry(mode, &connector->modes, head) {
			int  err      = -1;
			bool no_match = false;

			mode_id ++;

			if (!mode)
				continue;

			/* no matching mode ? */
			if (!mode_match) {

				/* fb smaller than mode is denied by drm_mode_setcrtc */
				if (fb_smaller_mode(&report_fb_info, mode))
					continue;

				/* use first smaller mode */
				mode_match = mode;

				if (conf_mode.enabled && conf_mode.id)
					no_match = true;
			}

			if (mode_match != mode)
				continue;

			/* convert kernel internal mode to user mode expectecd via ioctl */
			drm_mode_convert_to_umode(&user_mode, mode);

			/* assign fb & connector to crtc with specified mode */
			err = user_attach_fb_to_crtc(dev, connector, mode_set->crtc,
			                             &user_mode, dumb_fb.fb_id,
			                             conf_mode.enabled);

			if (err)
				retry = true;
			else
				report_fb = true;

			/* set brightness */
			if (!err && conf_mode.enabled && conf_mode.brightness <= MAX_BRIGHTNESS) {
				drm_modeset_lock(&dev->dev->mode_config.connection_mutex, NULL);
				set_brightness(conf_mode.enabled ? conf_mode.brightness : 0,
				               connector);
				drm_modeset_unlock(&dev->dev->mode_config.connection_mutex);
			}

			/* diagnostics */
			printk("%10s: %s name='%9s' id=%u%s mode=%4ux%4u@%u%s fb=%4ux%4u%s",
			       connector->name ? connector->name : "unnamed",
			       conf_mode.enabled ? " enable" : "disable",
			       mode->name ? mode->name : "noname",
			       mode_id, mode_id < 10 ? " " : "", mode->hdisplay,
			       mode->vdisplay, drm_mode_vrefresh(mode),
			       drm_mode_vrefresh(mode) < 100 ? " ": "",
			       framebuffer.hdisplay, framebuffer.vdisplay,
			       (err || no_match) ? "" : "\n");

			if (no_match)
				printk(" - no mode match: %ux%u\n",
				       conf_mode.width,
				       conf_mode.height);
			if (err)
				printk(" - failed, error=%d\n", err);

			break;
		}
	}

	if (report_fb)
		user_register_fb(dev, &report_fb_info, &dumb_fb);

	return retry;
}


static int configure_connectors(void * data)
{
	unsigned retry_count = 0;

	while (true) {
		bool retry = reconfigure(dev_client);

		if (retry && retry_count < 3) {
			retry_count ++;

			printk("retry applying configuration in 1s\n");
			msleep(1000);
			continue;
		}

		retry_count = 0;

		if (lx_emul_i915_config_done_and_block())
			lx_emul_task_schedule(true /* block task */);
	}

	return 0;
}


void lx_user_init(void)
{
	int pid = kernel_thread(configure_connectors, NULL, CLONE_FS | CLONE_FILES);
	lx_user_task = find_task_by_pid_ns(pid, NULL);;
}


void lx_emul_i915_report(void * lx_data, void * genode_data)
{
	struct drm_client_dev *client = lx_data;

	struct drm_connector_list_iter conn_iter;

	struct drm_device const *dev       = client->dev;
	struct drm_connector    *connector = NULL;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		lx_emul_i915_report_connector(connector, genode_data,
		                              connector->name,
		                              connector->status != connector_status_disconnected,
		                              get_brightness(connector, INVALID_BRIGHTNESS));
	}
	drm_connector_list_iter_end(&conn_iter);
}


void lx_emul_i915_iterate_modes(void * lx_data, void * genode_data)
{
	struct drm_connector    *connector = lx_data;
	struct drm_display_mode *mode      = NULL;
	struct drm_display_mode *prev_mode = NULL;
	unsigned                 mode_id   = 0;

	/* mark modes as unavailable due to max_resolution enforcement */
	struct genode_mode conf_max_mode = { };
	lx_emul_i915_connector_config("dummy", &conf_max_mode);


	list_for_each_entry(mode, &connector->modes, head) {
		bool skip = false;

		mode_id ++;

		if (!mode)
			continue;

		/* skip duplicates - actually not really, some parameters varies ?! */
		if (prev_mode) {
			skip = (mode->hdisplay == prev_mode->hdisplay) &&
			       (mode->vdisplay == prev_mode->vdisplay) &&
			       (drm_mode_vrefresh(mode) == drm_mode_vrefresh(prev_mode)) &&
			       !strncmp(mode->name, prev_mode->name, DRM_DISPLAY_MODE_LEN);
		}

		if (!skip) {
			bool const max_mode = conf_max_mode.max_width && conf_max_mode.max_height;

			struct genode_mode conf_mode = {
				.width = mode->hdisplay,
				.height = mode->vdisplay,
				.preferred = mode->type & (DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DEFAULT),
				.hz = drm_mode_vrefresh(mode),
				.id = mode_id,
				.enabled = !max_mode || !conf_smaller_max_mode(&conf_max_mode, mode)
			};

			static_assert(sizeof(conf_mode.name) == DRM_DISPLAY_MODE_LEN);
			memcpy(conf_mode.name, mode->name, sizeof(conf_mode.name));

			lx_emul_i915_report_modes(genode_data, &conf_mode);
		}

		prev_mode = mode;
	}
}


int i915_switcheroo_register(struct drm_i915_private *i915_private)
{
	return 0;
}


void i915_switcheroo_unregister(struct drm_i915_private *i915)
{
	return;
}


static int fb_client_hotplug(struct drm_client_dev *client)
{
	struct drm_mode_set    *modeset = NULL;
	struct drm_framebuffer *fb      = NULL;
	int    result                   = -EINVAL;

	if (dumb_fb.fb_id)
		fb = drm_framebuffer_lookup(client->dev, client->file, dumb_fb.fb_id);

	/*
	 * Triggers set up of display pipelines for connectors and
	 * stores the config in the client's modeset array.
	 */
	result = drm_client_modeset_probe(client, 0 /* auto width */,
	                                          0 /* auto height */);
	if (result) {
		printk("%s: error on modeset probe %d\n", __func__, result);
		return result;
	}

	/*
	 * (Re-)assign framebuffer to modeset (lost due to modeset_probe) and
	 * commit the change.
	 */
	if (fb) {
		mutex_lock(&client->modeset_mutex);
		drm_client_for_each_modeset(modeset, client) {
			if (!modeset || !modeset->num_connectors)
				continue;

			modeset->fb = fb;
		}
		mutex_unlock(&client->modeset_mutex);

		/* triggers disablement of encoders attached to disconnected ports */
		result = drm_client_modeset_commit(client);

		if (result) {
			printk("%s: error on modeset commit %d%s\n", __func__, result,
			       (result == -ENOSPC) ? " - ENOSPC" : " - unknown error");
		}
	}

	/* notify Genode side */
	lx_emul_i915_hotplug_connector(client);

	if (fb)
		drm_framebuffer_put(fb);

	return result;
}


static const struct drm_client_funcs drm_client_funcs = {
	.owner		= THIS_MODULE,
	.hotplug	= fb_client_hotplug,
/*
	.unregister	=
	.restore	=
*/
};


static int register_drm_client(struct drm_device * const dev)
{
	int result = -EINVAL;

	dev_client = kzalloc(sizeof(*dev_client), GFP_KERNEL);
	if (!dev_client) {
		drm_err(dev, "Failed to allocate drm_client_dev\n");
		return -ENOMEM;
	}

	result = drm_client_init(dev, dev_client, "genode_client",
	                         &drm_client_funcs);

	/* dev_client->file contains drm_file */
	if (result) {
		kfree(dev_client);
		drm_err(dev, "Failed to register client: %d\n", result);
		return -ENODEV;
	}

	drm_client_register(dev_client);

	/*
	 * Normally set via drm_ioctl() calling 'static int drm_setclientcap()'
	 *
	 * Without this feature bit set, drm_mode_setcrtc() denies usage of
	 * some modes we report as available.
	 */
	dev_client->file->aspect_ratio_allowed = 1;

	return 0;
}


int user_attach_fb_to_crtc(struct drm_client_dev          * const dev,
                           struct drm_connector     const * const connector,
                           struct drm_crtc          const * const crtc,
                           struct drm_mode_modeinfo const * const mode,
                           unsigned                         const fb_id,
                           bool                             const enable)
{
	int                  result         = -EINVAL;
	uint32_t             connectors [1] = { connector->base.id };
	struct drm_mode_crtc crtc_req       = {
		.set_connectors_ptr = (uintptr_t)(&connectors),
		.count_connectors   = enable ? 1 : 0,
		.crtc_id            = crtc->base.id,
		.fb_id              = fb_id,
		.x                  = 0,
		.y                  = 0, /* position on the framebuffer */
		.gamma_size         = 0,
		.mode_valid         = enable,
		.mode               = *mode,
	};

	result = drm_mode_setcrtc(dev->dev, &crtc_req, dev->file);
	if (result)
		drm_err(dev->dev, "%s: failed to set crtc %d\n", __func__, result);

	return result;
}


static int user_register_fb(struct drm_client_dev   const * const dev,
                            struct fb_info                * const info,
                            struct drm_mode_fb_cmd2 const * const dumb_fb)
{
	intel_wakeref_t wakeref;

	int                        result   = -EINVAL;
	struct i915_gtt_view const view     = { .type = I915_GTT_VIEW_NORMAL };
	static struct i915_vma    *vma      = NULL;
	static unsigned long       flags    = 0;
	void   __iomem            *vaddr    = NULL;
	struct drm_i915_private   *dev_priv = to_i915(dev->dev);
	struct drm_framebuffer    *fb       = drm_framebuffer_lookup(dev->dev,
	                                                             dev->file,
	                                                             dumb_fb->fb_id);

	if (!info || !fb) {
		printk("%s:%u error setting up info and fb\n", __func__, __LINE__);
		return -ENODEV;
	}

	wakeref = intel_runtime_pm_get(&dev_priv->runtime_pm);

	if (vma) {
		intel_unpin_fb_vma(vma, flags);

		vma   = NULL;
		flags = 0;
	}

	/* Pin the GGTT vma for our access via info->screen_base.
	 * This also validates that any existing fb inherited from the
	 * BIOS is suitable for own access.
	 */
	vma = intel_pin_and_fence_fb_obj(fb, false /* phys_cursor */,
	                                 &view, false /* use fences */,
	                                 &flags);

	if (IS_ERR(vma)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(vma);
			printk("%s:%u error setting vma %d\n", __func__, __LINE__, result);
		return result;
	}

	vaddr = i915_vma_pin_iomap(vma);
	if (IS_ERR(vaddr)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(vaddr);
		printk("%s:%u error pin iomap %d\n", __func__, __LINE__, result);
		return result;
	}

	/* fill framebuffer info for register_framebuffer */
	info->screen_base        = vaddr;
	info->screen_size        = vma->size;
	info->fix.line_length    = fb->pitches[0];
	info->var.bits_per_pixel = drm_format_info_bpp(fb->format, 0);

	intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

	register_framebuffer(info);

	if (fb)
		drm_framebuffer_put(fb);

	return 0;
}


static int check_resize_fb(struct drm_client_dev       * const dev,
                           struct drm_mode_create_dumb * const gem_dumb,
                           struct drm_mode_fb_cmd2     * const dumb_fb,
                           unsigned                      const width,
                           unsigned                      const height)
{
	int result = -EINVAL;

	/* paranoia */
	if (!dev || !dev->dev || !dev->file || !gem_dumb || !dumb_fb)
		return -ENODEV;

	/* if requested size is smaller, free up current dumb buffer */
	if (gem_dumb->width && gem_dumb->height &&
	    (gem_dumb->width < width || gem_dumb->height < height)) {

		result = drm_mode_rmfb(dev->dev, dumb_fb->fb_id, dev->file);
		if (result) {
			drm_err(dev->dev, "%s: failed to remove framebufer %d\n",
			        __func__, result);
		}

		result = drm_mode_destroy_dumb(dev->dev, gem_dumb->handle, dev->file);
		if (result) {
			drm_err(dev->dev, "%s: failed to destroy framebuffer %d\n",
			        __func__, result);
		}

		memset(gem_dumb, 0, sizeof(*gem_dumb));
		memset(dumb_fb,  0, sizeof(*dumb_fb));
	}

	/* allocate dumb framebuffer, on success a GEM object handle is returned */
	if (!gem_dumb->width && !gem_dumb->height) {
		gem_dumb->height = height;
		gem_dumb->width  = width;
		gem_dumb->bpp    = 32;
		gem_dumb->flags  = 0;
		/* .handle, .pitch, .size written by kernel in gem_dumb */

		result = drm_mode_create_dumb_ioctl(dev->dev, gem_dumb, dev->file);
		if (result) {
			drm_err(dev->dev, "%s: failed to create framebuffer %d\n",
			        __func__, result);
			memset(gem_dumb, 0, sizeof(*gem_dumb));
			return -ENODEV;
		}
	}

	/* bind framebuffer(GEM object) to drm client */
	if (!dumb_fb->width && !dumb_fb->height) {
		/* .fb_id <- written by kernel */
		dumb_fb->width        = gem_dumb->width,
		dumb_fb->height       = gem_dumb->height,
		dumb_fb->pixel_format = DRM_FORMAT_XRGB8888,
		/* .flags */
		/* up to 4 planes with handle/pitch/offset/modifier can be set */
		dumb_fb->handles[0] = gem_dumb->handle;
		dumb_fb->pitches[0] = gem_dumb->pitch;
		/* .offsets[4]  */
		/* .modifier[4] */

		result = drm_mode_addfb2_ioctl(dev->dev, dumb_fb, dev->file);
		if (result) {
			drm_err(dev->dev, "%s: failed to add framebuffer to drm client %d\n",
			        __func__, result);
			memset(dumb_fb, 0, sizeof(*dumb_fb));
			return -ENODEV;
		}
	}

	return 0;
}


int intel_fbdev_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);

	if (drm_WARN_ON(dev, !HAS_DISPLAY(dev_priv)))
		return -ENODEV;

	return register_drm_client(dev);
}


void intel_fbdev_fini(struct drm_i915_private *dev_priv)
{
	lx_emul_trace(__func__);
}


void intel_fbdev_initial_config_async(struct drm_device *dev)
{
	lx_emul_trace(__func__);
}


void intel_fbdev_unregister(struct drm_i915_private *dev_priv)
{
	lx_emul_trace(__func__);
}


void intel_fbdev_set_suspend(struct drm_device *dev, int state, bool synchronous)
{
	lx_emul_trace(__func__);
}


void intel_fbdev_restore_mode(struct drm_device *dev)
{
	lx_emul_trace(__func__);
}


void intel_fbdev_output_poll_changed(struct drm_device *dev)
{
	lx_emul_trace(__func__);
}
