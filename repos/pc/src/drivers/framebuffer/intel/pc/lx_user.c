/*
 * \brief  Post kernel activity
 * \author Alexander Boettcher
 * \date   2022-03-08
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched/task.h>

#include <drm/drm_fb_helper.h>
#include <drm/drm_print.h>

#include "i915_drv.h"
#include "display/intel_display_types.h"
#include "display/intel_opregion.h"
#include "display/intel_panel.h"

#include "lx_emul.h"


enum { MAX_BRIGHTNESS = 100, INVALID_BRIGHTNESS = MAX_BRIGHTNESS + 1 };

struct task_struct * lx_user_task = NULL;

static struct drm_i915_private *i915 = NULL;

static struct drm_fb_helper * i915_fb(void) { return &i915->fbdev->helper; }


/*
 * Heuristic to calculate max resolution across all connectors
 */
static void preferred_mode(struct drm_display_mode *prefer, uint64_t smaller_as)
{
	struct drm_connector          *connector  = NULL;
	struct drm_display_mode       *mode       = NULL;
	struct drm_connector_list_iter conn_iter;

	/* read Genode's config per connector */
	drm_connector_list_iter_begin(i915_fb()->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		struct genode_mode conf_mode = { .enabled = 1 };

		/* check for connector configuration on Genode side */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		if (!conf_mode.enabled)
			continue;

		if (conf_mode.id) {
			unsigned mode_id = 0;
			list_for_each_entry(mode, &connector->modes, head) {
				mode_id ++;

				if (!mode || conf_mode.id != mode_id)
					continue;

				conf_mode.width  = mode->hdisplay;
				conf_mode.height = mode->vdisplay;

				break;
			}
		}

		/* maximal resolution enforcement */
		if (conf_mode.max_width && conf_mode.max_height) {
			if (conf_mode.max_width * conf_mode.height < smaller_as)
				smaller_as = conf_mode.max_width * conf_mode.max_height + 1;

			if (conf_mode.max_width * conf_mode.height < prefer->hdisplay * prefer->vdisplay)
				continue;
		}

		if (!conf_mode.width || !conf_mode.height)
			continue;

		if (conf_mode.width * conf_mode.height > prefer->hdisplay * prefer->vdisplay) {
			prefer->hdisplay = conf_mode.width;
			prefer->vdisplay = conf_mode.height;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* too large check */
	if (smaller_as <= (uint64_t)prefer->hdisplay * prefer->vdisplay) {
		prefer->hdisplay = 0;
		prefer->vdisplay = 0;
	}

	/* if too large or nothing configured by Genode's config */
	if (!prefer->hdisplay || !prefer->vdisplay) {
		drm_connector_list_iter_begin(i915_fb()->dev, &conn_iter);
		drm_client_for_each_connector_iter(connector, &conn_iter) {
			list_for_each_entry(mode, &connector->modes, head) {
				if (!mode)
					continue;

				if (smaller_as <= (uint64_t)mode->hdisplay * mode->vdisplay)
					continue;

				if (mode->hdisplay * mode->vdisplay > prefer->hdisplay * prefer->vdisplay) {
					prefer->hdisplay = mode->hdisplay;
					prefer->vdisplay = mode->vdisplay;
				}
			}
		}
		drm_connector_list_iter_end(&conn_iter);
	}
}


static void set_brightness(unsigned brightness, struct drm_connector * connector)
{
	struct intel_connector * intel_c = to_intel_connector(connector);
	if (intel_c)
		intel_panel_set_backlight_acpi(intel_c->base.state, brightness, MAX_BRIGHTNESS);
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


static bool reconfigure(void * data)
{
	static uint64_t width_smaller_as  = 100000;
	static uint64_t height_smaller_as = 100000;

	struct drm_display_mode *mode           = NULL;
	struct drm_display_mode  mode_preferred = {};
	struct drm_mode_set     *mode_set       = NULL;
	struct fb_info           report_fb_info = {};
	bool                     report_fb      = false;
	bool                     retry          = false;

	if (!i915_fb())
		return retry;

	BUG_ON(!i915_fb()->funcs);
	BUG_ON(!i915_fb()->funcs->fb_probe);

	preferred_mode(&mode_preferred, width_smaller_as * height_smaller_as);

	if (mode_preferred.hdisplay && mode_preferred.vdisplay) {
		unsigned err = 0;
		struct drm_fb_helper_surface_size sizes = {};

		sizes.surface_depth  = 24;
		sizes.surface_bpp    = 32;
		sizes.fb_width       = mode_preferred.hdisplay;
		sizes.fb_height      = mode_preferred.vdisplay;
		sizes.surface_width  = sizes.fb_width;
		sizes.surface_height = sizes.fb_height;

		err = (*i915_fb()->funcs->fb_probe)(i915_fb(), &sizes);
		/* i915_fb()->fb contains adjusted drm_framebuffer object */

		if (err || !i915_fb()->fbdev) {
			printk("setting up framebuffer of %ux%u failed - error=%d\n",
			       mode_preferred.hdisplay, mode_preferred.vdisplay, err);

			if (err == -ENOMEM) {
				/*
				 * roll back code for intelfb_create() in
				 * drivers/gpu/drm/i915/display/intel_fbdev.c:
				 *
				 * vma = intel_pin_and_fence_fb_obj(&ifbdev->fb->base, false,
				 *                                  &view, false, &flags);
				 * if (IS_ERR(vma)) {
				 *
				 * If the partial allocation is not reverted, the next
				 * i915_fb()->funcs->fb_probe (which calls intelfb_create)
				 * will try the old resolution, which failed and fails again,
				 * instead of using the new smaller resolution.
				 */
				struct intel_fbdev *ifbdev =
					container_of(i915_fb(), struct intel_fbdev, helper);

				if (ifbdev && ifbdev->fb) {
					drm_framebuffer_put(&ifbdev->fb->base);
					ifbdev->fb = NULL;
				}

				width_smaller_as  = mode_preferred.hdisplay;
				height_smaller_as = mode_preferred.vdisplay;

				retry = true;
				return retry;
			}
		} else {
			width_smaller_as  = 100000;
			height_smaller_as = 100000;
		}
	}

	if (!i915_fb()->fb)
		return retry;

	/* data is adjusted if virtual resolution is not same size as physical fb */
	report_fb_info = *i915_fb()->fbdev;
	if (mode_preferred.hdisplay && mode_preferred.vdisplay) {
		report_fb_info.var.xres_virtual = mode_preferred.hdisplay;
		report_fb_info.var.yres_virtual = mode_preferred.vdisplay;
	}

	drm_client_for_each_modeset(mode_set, &(i915_fb()->client)) {
		struct drm_display_mode *mode_match = NULL;
		unsigned                 mode_id    = 0;
		struct drm_connector    *connector  = NULL;

		struct genode_mode conf_mode = { .enabled = 1,
		                                 .brightness = INVALID_BRIGHTNESS };

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
			if (report_fb_info.var.xres * report_fb_info.var.yres <
			    mode->vdisplay * mode->hdisplay)
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
			struct drm_mode_set set;
			int                 err      = -1;
			bool                no_match = false;

			mode_id ++;

			if (!mode)
				continue;

			/* no matching mode ? */
			if (!mode_match) {

				/* allocated framebuffer smaller than mode can't be used */
				if (report_fb_info.var.xres * report_fb_info.var.yres <
				    mode->vdisplay * mode->hdisplay)
					continue;

				/* use first smaller mode */
				mode_match = mode;

				if (conf_mode.enabled)
					no_match = true;
			}

			if (mode_match != mode)
				continue;

			set.crtc           = mode_set->crtc;
			set.x              = 0;
			set.y              = 0;
			set.mode           = conf_mode.enabled ? mode : NULL;
			set.connectors     = &connector;
			set.num_connectors = conf_mode.enabled ? 1 : 0;
			set.fb             = conf_mode.enabled ? i915_fb()->fb : NULL;

			if (set.crtc->funcs && set.crtc->funcs->set_config &&
			    drm_drv_uses_atomic_modeset(i915_fb()->dev)) {

				struct drm_modeset_acquire_ctx ctx;

				DRM_MODESET_LOCK_ALL_BEGIN(i915_fb()->dev, ctx,
				                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
				                           err);
				err = set.crtc->funcs->set_config(&set, &ctx);

				if (!err && conf_mode.enabled && conf_mode.brightness <= MAX_BRIGHTNESS)
					set_brightness(conf_mode.brightness, connector);

				DRM_MODESET_LOCK_ALL_END(i915_fb()->dev, ctx, err);

				if (err)
					retry = true;
				else {
					report_fb = true;
				}
			}

			printk("%s: %s name='%s' id=%u %ux%u@%u%s",
			       connector->name ? connector->name : "unnamed",
			       conf_mode.enabled ? " enable" : "disable",
			       mode->name ? mode->name : "noname",
			       mode_id, mode->hdisplay,
			       mode->vdisplay, drm_mode_vrefresh(mode),
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
		register_framebuffer(&report_fb_info);

	return retry;
}


static int configure_connectors(void * data)
{
	unsigned retry_count = 0;

	while (true) {
		bool retry = reconfigure(data);

		if (retry && retry_count < 3) {
			retry_count ++;

			printk("retry applying configuration in 1s\n");
			msleep(1000);
			continue;
		}

		retry_count = 0;

		lx_emul_task_schedule(true);
	}

	return 0;
}


void lx_user_init(void)
{
	int pid = kernel_thread(configure_connectors, NULL, CLONE_FS | CLONE_FILES);
	lx_user_task = find_task_by_pid_ns(pid, NULL);;
}


static int genode_fb_client_hotplug(struct drm_client_dev *client)
{
	/*
	 * Set deferred_setup to execute codepath of drm_fb_helper_hotplug_event()
	 * on next connector state change that does not drop modes, which are
	 * above the current framebuffer resolution. It is required if the
	 * connected display at runtime is larger than the ones attached already
	 * during boot. Without this quirk, not all modes are reported on displays
	 * connected after boot.
	 */
	i915_fb()->deferred_setup = true;

	lx_emul_i915_hotplug_connector(client);
	return 0;
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
			struct genode_mode conf_mode = { .width = mode->hdisplay,
			                                 .height = mode->vdisplay,
			                                 .preferred = mode->type & (DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DEFAULT),
			                                 .hz = drm_mode_vrefresh(mode),
			                                 .id = mode_id
			                               };

			static_assert(sizeof(conf_mode.name) == DRM_DISPLAY_MODE_LEN);
			memcpy(conf_mode.name, mode->name, sizeof(conf_mode.name));

			lx_emul_i915_report_modes(genode_data, &conf_mode);
		}

		prev_mode = mode;
	}
}


static const struct drm_client_funcs drm_fbdev_client_funcs = {
	.owner		= THIS_MODULE,
	.hotplug	= genode_fb_client_hotplug,
};


static void hotplug_setup(struct drm_device *dev)
{
	struct drm_fb_helper *hotplug_helper;
	int ret;

	hotplug_helper = kzalloc(sizeof(*hotplug_helper), GFP_KERNEL);
	if (!hotplug_helper) {
		drm_err(dev, "Failed to allocate fb_helper\n");
		return;
	}

	ret = drm_client_init(dev, &hotplug_helper->client, "fbdev",
	                      &drm_fbdev_client_funcs);
	if (ret) {
		kfree(hotplug_helper);
		drm_err(dev, "Failed to register client: %d\n", ret);
		return;
	}

	hotplug_helper->preferred_bpp = 32;

	ret = genode_fb_client_hotplug(&hotplug_helper->client);
	if (ret)
		drm_dbg_kms(dev, "client hotplug ret=%d\n", ret);

	drm_client_register(&hotplug_helper->client);

	hotplug_helper->dev = dev;
}


int i915_switcheroo_register(struct drm_i915_private *i915_private)
{
	/* get hold of the function pointers we need for mode setting */
	i915 = i915_private;

	/* register dummy fb_helper to get notifications about hotplug events */
	hotplug_setup(&i915_private->drm);

	return 0;
}


void i915_switcheroo_unregister(struct drm_i915_private *i915)
{
	lx_emul_trace_and_stop(__func__);
}
