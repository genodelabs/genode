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
static void preferred_mode(struct drm_display_mode *prefer)
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

		if (!conf_mode.enabled || !conf_mode.width || !conf_mode.height)
			continue;

		if (conf_mode.width * conf_mode.height > prefer->hdisplay * prefer->vdisplay) {
			prefer->hdisplay = conf_mode.width;
			prefer->vdisplay = conf_mode.height;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* if nothing was configured by Genode's config, apply heuristic */
	if (!prefer->hdisplay || !prefer->vdisplay) {
		drm_connector_list_iter_begin(i915_fb()->dev, &conn_iter);
		drm_client_for_each_connector_iter(connector, &conn_iter) {
			list_for_each_entry(mode, &connector->modes, head) {
				if (!mode)
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

	preferred_mode(&mode_preferred);

	if (mode_preferred.hdisplay && mode_preferred.vdisplay) {
		unsigned err = 0;
		struct drm_fb_helper_surface_size sizes = {};

		sizes.surface_depth  = 32;
		sizes.surface_bpp    = 32;
		sizes.fb_width       = mode_preferred.hdisplay;
		sizes.fb_height      = mode_preferred.vdisplay;
		sizes.surface_width  = sizes.fb_width;
		sizes.surface_height = sizes.fb_height;

		err = (*i915_fb()->funcs->fb_probe)(i915_fb(), &sizes);
		/* i915_fb()->fb contains adjusted drm_frambuffer object */

		if (err || !i915_fb()->fbdev)
			printk("setting up framebuffer failed - error=%d\n", err);
	}

	if (!i915_fb()->fb)
		return retry;

	/* data is adjusted if virtual resolution is not same size as physical fb */
	report_fb_info = *i915_fb()->fbdev;

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

			/* use mode id if configured and matches exactly */
			if (conf_mode.id && (conf_mode.id == mode_id)) {
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
				/* use first mode */
				mode_match = mode;
				/* set up preferred resolution as virtual, if nothing is enforced */
				if (!conf_mode.preferred &&
					mode_preferred.hdisplay &&
					mode_preferred.vdisplay) {
					conf_mode.preferred = 1;
					conf_mode.width  = mode_preferred.hdisplay;
					conf_mode.height = mode_preferred.vdisplay;
				}
				no_match = mode->hdisplay != conf_mode.width ||
				           mode->vdisplay != conf_mode.height;
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

					/* report forced resolution */
					if (conf_mode.preferred) {
						report_fb_info.var.xres_virtual = conf_mode.width;
						report_fb_info.var.yres_virtual = conf_mode.height;
					}
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
		                              !!connector->encoder /* connected */,
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
