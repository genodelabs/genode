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


       struct task_struct    * lx_user_task   = NULL;
static struct task_struct    * lx_update_task = NULL;
static struct drm_client_dev * dev_client     = NULL;


static int user_register_fb(struct drm_client_dev   const * const dev,
                            struct fb_info                * const info,
                            struct drm_mode_fb_cmd2 const * const dumb_fb,
                            unsigned width_mm, unsigned height_mm);


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
 * Heuristic to calculate max resolution across all mirrored connectors
 */
static void preferred_mirror(struct drm_device const * const dev,
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
		struct genode_mode      conf_mode = { };
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

		if (conf_mode.mirror      &&
		    conf_mode.force_width && conf_mode.force_height) {

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

		if (!conf_mode.width || !conf_mode.height || !conf_mode.mirror)
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

		struct genode_mode conf_mode = { };
		/* check for connector configuration on Genode side */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		list_for_each_entry(mode, &connector->modes, head) {
			if (!mode)
				continue;

			if (mode_larger(min_mode, mode))
				continue;

			if (max_enforcement.hdisplay && max_enforcement.vdisplay) {
				if (mode_larger(mode, &max_enforcement))
					continue;
			}

			if (conf_mode.enabled && !conf_mode.mirror)
				continue;

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


static struct drm_mode_fb_cmd2  *mirror_fb_cmd;


static struct drm_framebuffer * lookup_framebuffer(struct drm_crtc *crtc,
                                                   struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_plane_state  *plane;
	struct drm_crtc_state   *crtc_state;

	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return NULL;

	state->acquire_ctx = ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		drm_atomic_state_put(state);
		return NULL;
	}

	plane = drm_atomic_get_plane_state(state, crtc->primary);

	drm_atomic_state_put(state);

	return plane ? plane->fb : NULL;
}


enum { MAX_FBS = 8 };


/* own data structure for tracking dumb buffers, e.g. GEM handles, flags, vma */
struct gem_dumb {
	struct drm_mode_create_dumb fb_dumb;
	struct drm_mode_fb_cmd2     fb_cmd;
	struct i915_vma           * vma;
	unsigned long               flags;
};


/* allocator and lookup helper for our own meta data */
static bool _meta_data(struct drm_client_dev  const * const dev,
                       struct drm_framebuffer const * const fb,
                       struct drm_mode_create_dumb ** fb_dumb,  /* out */
                       struct drm_mode_fb_cmd2     ** fb_cmd,   /* out */
                       struct gem_dumb             ** gem_dumb) /* out */
{
	static struct gem_dumb gem_dumb_list [MAX_FBS] = { };

	struct gem_dumb * free_slot = NULL;

	if (!dev)
		return false;

	for (unsigned i = 0; i < MAX_FBS; i++) {
		struct gem_dumb        * slot = &gem_dumb_list[i];
		struct drm_framebuffer * cmp  = NULL;

		if (!slot->fb_cmd.fb_id) {
			if (!free_slot)
				free_slot = slot;

			continue;
		}

		if (!fb)
			continue;

		cmp = drm_framebuffer_lookup(dev->dev, dev->file,
		                             slot->fb_cmd.fb_id);

		if (cmp)
			drm_framebuffer_put(cmp);

		/* lookup case */
		if (cmp == fb) {
			if (fb_dumb)  *fb_dumb  = &slot->fb_dumb;
			if (fb_cmd)   *fb_cmd   = &slot->fb_cmd;
			if (gem_dumb) *gem_dumb =  slot;
			return true;
		}
	}

	/* allocate case */
	if (free_slot) {
		if (fb_dumb)  *fb_dumb  = &free_slot->fb_dumb;
		if (fb_cmd)   *fb_cmd   = &free_slot->fb_cmd;
		if (gem_dumb) *gem_dumb =  free_slot;
	}

	return !!free_slot;
}


static bool dumb_meta(struct drm_client_dev  const * const dev,
                      struct drm_framebuffer const * const fb,
                      struct drm_mode_create_dumb ** fb_dumb, /* out */
                      struct drm_mode_fb_cmd2     ** fb_cmd)  /* out */
{
	return _meta_data(dev, fb, fb_dumb, fb_cmd, NULL);
}


static bool dumb_gem(struct drm_client_dev  const * const dev,
                     struct drm_framebuffer const * const fb,
                     struct gem_dumb             ** gem_dumb) /* out */
{
	return _meta_data(dev, fb, NULL, NULL, gem_dumb);
}


static void destroy_fb(struct drm_client_dev       * const dev,
                       struct drm_mode_create_dumb * const gem_dumb,
                       struct drm_mode_fb_cmd2     * const dumb_fb)
{
	int result = drm_mode_rmfb(dev->dev, dumb_fb->fb_id, dev->file);

	if (result) {
		drm_err(dev->dev, "%s: failed to remove framebuffer %d\n",
		        __func__, result);
	}

	result = drm_mode_destroy_dumb(dev->dev, gem_dumb->handle, dev->file);

	if (result) {
		drm_err(dev->dev, "%s: failed to destroy framebuffer %d\n",
		        __func__, result);
	}

	/* frees the entry in _meta_data allocator */
	memset(gem_dumb, 0, sizeof(*gem_dumb));
	memset(dumb_fb,  0, sizeof(*dumb_fb));
}


static int kernel_register_fb(struct fb_info const * const fb_info,
                              unsigned               const width_mm,
                              unsigned               const height_mm)
{
	lx_emul_i915_framebuffer_ready(fb_info->node,
	                               fb_info->par,
	                               fb_info->screen_base,
	                               fb_info->screen_size,
	                               fb_info->var.xres_virtual,
	                               fb_info->var.yres_virtual,
	                               fb_info->fix.line_length /
	                               (fb_info->var.bits_per_pixel / 8),
	                               fb_info->var.yres,
	                               width_mm, height_mm);

	return 0;
}


struct drm_mode_fb_cmd2  fb_of_screen(struct drm_client_dev         * const dev,
                                      struct genode_mode      const * const conf_mode,
                                      struct fb_info                * const fb_info,
                                      struct drm_mode_fb_cmd2 const * const dumb_fb_mirror,
                                      struct drm_display_mode const * const mode,
                                      struct drm_framebuffer        *       fb,
                                      struct drm_connector    const * const connector)
{
	int                          err       = -EINVAL;
	struct drm_mode_create_dumb *gem_dumb  = NULL;
	struct drm_mode_fb_cmd2     *fb_cmd    = NULL;
	struct drm_framebuffer      *fb_mirror = drm_framebuffer_lookup(dev->dev,
	                                                                dev->file,
	                                                                dumb_fb_mirror->fb_id);

	/* during hotplug the mirrored fb is used for non mirrored connectors temporarily */
	if (fb && !conf_mode->mirror && fb == fb_mirror) {
		fb = NULL;
	}

	if (!dumb_meta(dev, fb, &gem_dumb, &fb_cmd) || !gem_dumb || !fb_cmd) {
		struct drm_mode_fb_cmd2 invalid = { };
		printk("could not create dumb buffer\n");
		return invalid;
	}

	/* notify genode side about switch from connector specific fb to mirror fb */
	if (fb && conf_mode->mirror && fb != fb_mirror) {
		struct fb_info info = {};

		info.var.bits_per_pixel = 32;
		info.node               = connector->index;
		info.par                = connector->name;

		kernel_register_fb(&info, mode->width_mm, mode->height_mm);

		destroy_fb(dev, gem_dumb, fb_cmd);
	}

	if (fb_mirror)
		drm_framebuffer_put(fb_mirror);

	fb_info->node        = connector->index;

	if (!conf_mode->enabled) {
		struct drm_mode_fb_cmd2 invalid = { };
		return invalid;
	}

	if (conf_mode->mirror)
		return *dumb_fb_mirror;

	err = check_resize_fb(dev, gem_dumb, fb_cmd,
	                      mode->hdisplay, mode->vdisplay);

	if (err) {
		printk("setting up framebuffer of %ux%u failed - error=%d\n",
		       mode->hdisplay, mode->vdisplay, err);
		return *dumb_fb_mirror;
	}

	fb_info->var.xres         = mode->hdisplay;
	fb_info->var.yres         = mode->vdisplay;
	fb_info->var.xres_virtual = mode->hdisplay;
	fb_info->var.yres_virtual = mode->vdisplay;

	return *fb_cmd;
}


static void close_unused_captures(struct drm_client_dev * const dev)
{
	/* report disconnected connectors to close capture connections */
	struct drm_connector_list_iter  conn_iter;
	struct drm_connector           *connector = NULL;

	drm_connector_list_iter_begin(dev->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		bool unused = connector->status != connector_status_connected;

		if (!unused) {
			unused = !connector->state || !connector->state->crtc;

			if (!unused) {
				struct drm_modeset_acquire_ctx ctx;
				void * fb  = NULL;
				int    err = -1;

				DRM_MODESET_LOCK_ALL_BEGIN(dev->dev, ctx,
				                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
				                           err);

				fb = lookup_framebuffer(connector->state->crtc, &ctx);

				DRM_MODESET_LOCK_ALL_END(dev->dev, ctx, err);

				unused = !fb;
			}
		}

		if (unused) {
			struct fb_info fb_info = {};

			fb_info.var.bits_per_pixel = 32;
			fb_info.node               = connector->index;

			kernel_register_fb(&fb_info, 0, 0);
		}
	}
	drm_connector_list_iter_end(&conn_iter);
}


static bool reconfigure(struct drm_client_dev * const dev)
{
	static struct drm_mode_create_dumb *gem_mirror = NULL;

	struct drm_display_mode    mode_preferred = {};
	struct drm_display_mode    mode_minimum   = {};
	struct drm_display_mode    framebuffer    = {};
	struct drm_mode_modeinfo   user_mode      = {};
	struct drm_display_mode  * mode           = NULL;
	struct drm_mode_set      * mode_set       = NULL;
	struct fb_info             info           = {};
	bool                       retry          = false;
	struct {
		struct fb_info info;
		unsigned       width_mm;
		unsigned       height_mm;
		bool           report;
	} mirror = { { }, 0, 0, false };

	if (!gem_mirror) {
		/* request storage for gem_mirror and mirror_fb_cmd */
		dumb_meta(dev, NULL, &gem_mirror, &mirror_fb_cmd);
	}

	if (!dev || !dev->dev || !gem_mirror || !mirror_fb_cmd)
		return false;

	preferred_mirror(dev->dev, &mode_preferred, &mode_minimum);

	if (!mode_minimum.hdisplay || !mode_minimum.vdisplay) {
		/* no valid modes on any connector on early boot */
		if (!mirror_fb_cmd->fb_id)
			return false;

		/* valid connectors but all are disabled by config */
		mode_minimum.hdisplay = mirror_fb_cmd->width;
		mode_minimum.vdisplay = mirror_fb_cmd->height;
		mode_preferred        = mode_minimum;
	}

	if (mode_larger(&mode_preferred, &mode_minimum))
		framebuffer = mode_preferred;
	else
		framebuffer = mode_minimum;

	{
		int const err = check_resize_fb(dev,
		                                gem_mirror,
		                                mirror_fb_cmd,
		                                framebuffer.hdisplay,
		                                framebuffer.vdisplay);

		if (err) {
			printk("setting up framebuffer of %ux%u failed - error=%d\n",
			       framebuffer.hdisplay, framebuffer.vdisplay, err);

			return true;
		}
	}

	/* without fb handle created by check_resize_fb we can't proceed */
	if (!mirror_fb_cmd->fb_id)
		return retry;

	/* prepare fb info for kernel_register_fb() evaluated by Genode side */
	info.var.xres         = framebuffer.hdisplay;
	info.var.yres         = framebuffer.vdisplay;
	info.var.xres_virtual = mode_preferred.hdisplay;
	info.var.yres_virtual = mode_preferred.vdisplay;

	drm_client_for_each_modeset(mode_set, dev) {
		struct drm_display_mode * mode_match = NULL;
		unsigned                  mode_id    = 0;
		struct drm_connector    * connector  = NULL;
		struct genode_mode        conf_mode  = {};

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
			if (fb_smaller_mode(&info, mode))
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
			struct fb_info          fb_info  = info;
			int                     err      = -1;
			bool                    no_match = false;
			struct drm_mode_fb_cmd2 fb_cmd   = *mirror_fb_cmd;

			mode_id ++;

			if (!mode)
				continue;

			/* use first mode for non mirrored connector in case of no match */
			if (!mode_match && !conf_mode.mirror) {

				struct drm_display_mode max = { .hdisplay = conf_mode.max_width,
				                                .vdisplay = conf_mode.max_height };

				if (conf_mode.max_width && conf_mode.max_height) {
					if (conf_larger_mode(&conf_mode, &max))
						continue;
				}

				mode_match = mode;
			}

			/* no matching mode ? */
			if (!mode_match) {

				/* fb smaller than mode is denied by drm_mode_setcrtc */
				if (fb_smaller_mode(&fb_info, mode))
					continue;

				/* use first smaller mode */
				mode_match = mode;

				if (conf_mode.enabled && conf_mode.id)
					no_match = true;
			}

			if (mode_match != mode)
				continue;

			/* Genode side prefer to have a name for the connector */
			fb_info.par = connector->name;

			{
				struct drm_modeset_acquire_ctx ctx; 
				struct drm_framebuffer *fb = NULL;

				DRM_MODESET_LOCK_ALL_BEGIN(dev->dev, ctx,
				                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
				                           err);

				fb = lookup_framebuffer(mode_set->crtc, &ctx);

				DRM_MODESET_LOCK_ALL_END(dev->dev, ctx, err);

				/* check for mirrored fb or specific one for connector */
				fb_cmd = fb_of_screen(dev, &conf_mode, &fb_info, mirror_fb_cmd,
				                      mode, fb, connector);
			}

			/* convert kernel internal mode to user mode expectecd via ioctl */
			drm_mode_convert_to_umode(&user_mode, mode);

			/* assign fb & connector to crtc with specified mode */
			err = user_attach_fb_to_crtc(dev, connector, mode_set->crtc,
			                             &user_mode, fb_cmd.fb_id,
			                             conf_mode.enabled);

			/* set brightness */
			if (!err && conf_mode.enabled && conf_mode.brightness <= MAX_BRIGHTNESS) {
				drm_modeset_lock(&dev->dev->mode_config.connection_mutex, NULL);
				set_brightness(conf_mode.enabled ? conf_mode.brightness : 0,
				               connector);
				drm_modeset_unlock(&dev->dev->mode_config.connection_mutex);
			}

			if (!retry)
				retry = !!err;

			if (!err && conf_mode.mirror && !mirror.report) {
				/* use fb_info of first mirrored screen */
				mirror.report    = true;
				mirror.width_mm  = mode->width_mm;
				mirror.height_mm = mode->height_mm;
				mirror.info      = fb_info;
			}

			/* diagnostics */
			printk("%10s: %s name='%9s' id=%u%s%s mode=%4ux%4u@%u%s fb=%4ux%4u%s",
			       connector->name ? connector->name : "unnamed",
			       conf_mode.enabled ? " enable" : "disable",
			       mode->name ? mode->name : "noname",
			       mode_id, mode_id < 10 ? " " : "",
			       conf_mode.mirror ? " mirror  " : " discrete",
			       mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode),
			       drm_mode_vrefresh(mode) < 100 ? " ": "",
			       framebuffer.hdisplay, framebuffer.vdisplay,
			       (err || no_match) ? "" : "\n");

			if (no_match)
				printk(" - no mode match: %ux%u\n",
				       conf_mode.width,
				       conf_mode.height);
			if (err)
				printk(" - failed, error=%d\n", err);

			if (!err && !conf_mode.mirror)
				user_register_fb(dev, &fb_info, &fb_cmd,
				                 mode->width_mm, mode->height_mm);

			break;
		}
	}

	if (mirror.report) {
		mirror.info.par = "mirror_capture";
		user_register_fb(dev, &mirror.info, mirror_fb_cmd, mirror.width_mm,
		                 mirror.height_mm);
	}

	close_unused_captures(dev);

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


static void mark_framebuffer_dirty(struct drm_framebuffer * const fb)
{
	struct drm_clip_rect        *clips = NULL;
	struct drm_mode_fb_dirty_cmd r     = { };

	unsigned flags     = 0;
	int      num_clips = 0;
	int      ret       = 0;

	if (!dev_client)
		return;

	if (!fb || !fb->funcs || !fb->funcs->dirty)
		return;

	ret = fb->funcs->dirty(fb, dev_client->file, flags, r.color, clips,
	                       num_clips);

	if (ret)
		printk("%s failed %d\n", __func__, ret);
}


/* track per connector (16 max) the empty capture attempts before stopping */
enum { CAPTURE_RATE_MS = 10, ATTEMPTS_BEFORE_STOP = 7 };
static unsigned unchanged[16] = { };

void lx_emul_i915_wakeup(unsigned const connector_id)
{
	bool const valid_id  = connector_id < sizeof(unchanged) / sizeof(*unchanged);

	if (!valid_id) {
		printk("%s: connector id invalid %d\n", __func__, connector_id);
		return;
	}

	unchanged[connector_id] = 0;

	/* wake potential sleeping update task */
	lx_emul_task_unblock(lx_update_task);
}


static int update_content(void *)
{
	while (true) {
		struct drm_connector_list_iter  conn_iter;
		struct drm_connector           *connector  = NULL;
		struct drm_device const        *dev        = dev_client->dev;
		bool                            block_task = true;

		drm_connector_list_iter_begin(dev, &conn_iter);
		drm_client_for_each_connector_iter(connector, &conn_iter) {

			struct drm_modeset_acquire_ctx  ctx;
			struct drm_framebuffer         *fb  = NULL;

			int        err       = -1;
			bool       may_sleep = false;
			bool const valid_id  = connector->index < sizeof(unchanged) / sizeof(*unchanged);

			if (connector->status != connector_status_connected)
				continue;

			if (valid_id)
				unchanged[connector->index] ++;
			else
				printk("%s: connector id invalid %d\n", __func__, connector->index);

			if (valid_id)
				may_sleep = unchanged[connector->index] >= ATTEMPTS_BEFORE_STOP;

			if (!lx_emul_i915_blit(connector->index, may_sleep)) {
				if (!may_sleep)
					block_task = false;

				continue;
			}

			block_task = false;

			if (valid_id)
				unchanged[connector->index] = 0;

			if (!connector->state || !connector->state->crtc)
				continue;

			DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx,
			                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
			                           err);

			fb = lookup_framebuffer(connector->state->crtc, &ctx);

			DRM_MODESET_LOCK_ALL_END(dev, ctx, err);

			if (fb)
				mark_framebuffer_dirty(fb);
		}
		drm_connector_list_iter_end(&conn_iter);

		if (block_task)
			lx_emul_task_schedule(true /* block task */);
		else
			/* schedule_timeout(jiffes) or hrtimer or msleep */
			msleep(CAPTURE_RATE_MS);
	}

	return 0;
}


void lx_user_init(void)
{
	int pid  = kernel_thread(configure_connectors, NULL, "lx_user",
	                         CLONE_FS | CLONE_FILES);
	int pid2 = kernel_thread(update_content, NULL, "lx_update",
	                         CLONE_FS | CLONE_FILES);

	lx_user_task   = find_task_by_pid_ns(pid , NULL);
	lx_update_task = find_task_by_pid_ns(pid2, NULL);
}


static bool mirrored_fb(struct drm_client_dev * client,
                        struct drm_crtc const * const crtc)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_framebuffer const * fb        = NULL;
	struct drm_framebuffer const * fb_mirror = NULL;
	int                            result    = -1;

	if (mirror_fb_cmd && mirror_fb_cmd->fb_id)
		fb_mirror = drm_framebuffer_lookup(client->dev, client->file,
		                                   mirror_fb_cmd->fb_id);

	if (!fb_mirror || !crtc)
		return false;

	if (fb_mirror)
		drm_framebuffer_put(fb_mirror);

	DRM_MODESET_LOCK_ALL_BEGIN(client->dev, ctx,
	                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
	                           result);

	fb = lookup_framebuffer(crtc, &ctx);

	DRM_MODESET_LOCK_ALL_END(client->dev, ctx, result);

	return fb && fb_mirror == fb;
}


static void _report_connectors(void * genode_data, bool const discrete)
{
	struct drm_connector_list_iter conn_iter;

	struct drm_device const *dev       = dev_client->dev;
	struct drm_connector    *connector = NULL;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {

		bool mirror = connector->state && connector->state->crtc &&
		              mirrored_fb(dev_client, connector->state->crtc);

		if (!mirror && (!connector->state || !connector->state->crtc)) {
			struct genode_mode conf_mode = { };

			/* check for connector configuration on Genode side */
			lx_emul_i915_connector_config(connector->name, &conf_mode);

			mirror = conf_mode.mirror;
		}

		if ((discrete && mirror) || (!discrete && !mirror))
			continue;

		lx_emul_i915_report_connector(connector, genode_data,
		                              connector->name,
		                              connector->status != connector_status_disconnected,
		                              get_brightness(connector, INVALID_BRIGHTNESS),
		                              connector->display_info.width_mm,
		                              connector->display_info.height_mm);
	}
	drm_connector_list_iter_end(&conn_iter);
}


void lx_emul_i915_report_discrete(void * genode_data)
{
	_report_connectors(genode_data, true /* discrete */);
}


void lx_emul_i915_report_non_discrete(void * genode_data)
{
	_report_connectors(genode_data, false /* non discrete */);
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
			bool const max_mode = conf_max_mode.max_width &&
			                      conf_max_mode.max_height;
			bool const inuse = connector->state && connector->state->crtc &&
			                   connector->state->crtc->state &&
			                   drm_mode_equal(&connector->state->crtc->state->mode, mode);
			bool const mirror = connector->state && connector->state->crtc &&
			                    mirrored_fb(dev_client, connector->state->crtc);

			struct genode_mode conf_mode = {
				.width     = mode->hdisplay,
				.height    = mode->vdisplay,
				.width_mm  = mode->width_mm,
				.height_mm = mode->height_mm,
				.preferred = mode->type & (DRM_MODE_TYPE_PREFERRED |
				                           DRM_MODE_TYPE_DEFAULT),
				.inuse     = inuse,
				.mirror    = mirror,
				.hz        = drm_mode_vrefresh(mode),
				.id        = mode_id,
				.enabled   = !max_mode ||
				             !conf_smaller_max_mode(&conf_max_mode, mode)
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
	struct drm_mode_set    *modeset   = NULL;
	struct drm_framebuffer *fb_mirror = NULL;
	int    result                     = -EINVAL;

	if (mirror_fb_cmd && mirror_fb_cmd->fb_id)
		fb_mirror = drm_framebuffer_lookup(client->dev, client->file,
		                                   mirror_fb_cmd->fb_id);

	/*
	 * Triggers set up of display pipelines for connectors and
	 * stores the config in the client's modeset array.
	 */
	result = drm_client_modeset_probe(client, 0 /* auto width */,
	                                          0 /* auto height */);
	if (result) {
		printk("%s: error on modeset probe %d\n", __func__, result);
		return 0;
	}

	/*
	 * (Re-)assign framebuffer to modeset (lost due to modeset_probe) and
	 * commit the change.
	 */
	if (fb_mirror) {
		struct   drm_framebuffer *       free_fbs[MAX_FBS] = { };
		struct   drm_modeset_acquire_ctx ctx;

		bool     mode_too_large = false;
		unsigned fb_count       = 0;

		DRM_MODESET_LOCK_ALL_BEGIN(client->dev, ctx,
		                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
		                           result);
		mutex_lock(&client->modeset_mutex);
		drm_client_for_each_modeset(modeset, client) {
			struct drm_connector   *connector  = NULL;
			struct drm_framebuffer *fb         = NULL;

			if (!modeset)
				continue;

			if (modeset->crtc)
				fb = lookup_framebuffer(modeset->crtc, &ctx);

			if (!mode_too_large && fb && modeset->mode &&
			    (modeset->mode->hdisplay > fb->width ||
			     modeset->mode->vdisplay > fb->height))
				mode_too_large = true;

			if (!modeset->num_connectors || !modeset->connectors || !*modeset->connectors) {

				struct drm_mode_fb_cmd2     *fb_cmd  = NULL;
				struct drm_mode_create_dumb *fb_dumb = NULL;

				if (!fb || fb == fb_mirror)
					continue;

				if (!dumb_meta(client, fb, &fb_dumb, &fb_cmd) || !fb_dumb || !fb_cmd)
					continue;

				if (fb_count >= MAX_FBS) {
					printk("leaking framebuffer memory\n");
					continue;
				}

				free_fbs[fb_count++] = fb;

				continue;
			}

			/* set connector */
			connector = *modeset->connectors;

			modeset->fb = fb ? fb : fb_mirror;
		}
		mutex_unlock(&client->modeset_mutex);
		DRM_MODESET_LOCK_ALL_END(client->dev, ctx, result);

		for (unsigned i = 0; i < fb_count; i++) {
			struct drm_mode_fb_cmd2     *fb_cmd  = NULL;
			struct drm_mode_create_dumb *fb_dumb = NULL;

			if (!free_fbs[i])
				continue;

			if (!dumb_meta(client, free_fbs[i], &fb_dumb, &fb_cmd) || !fb_dumb || !fb_cmd)
				continue;

			destroy_fb(client, fb_dumb, fb_cmd);

			free_fbs[i] = NULL;
		}

		/* triggers disablement of encoders attached to disconnected ports */
		result = drm_client_modeset_commit(client);

		if (result && !(mode_too_large && result == -ENOSPC)) {
			printk("%s: error on modeset commit %d%s\n", __func__, result,
			       (result == -ENOSPC) ? " - ENOSPC" : " - unknown error");
		}
	}

	/* notify Genode side */
	lx_emul_i915_hotplug_connector();

	if (fb_mirror)
		drm_framebuffer_put(fb_mirror);

	return 0;
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
                            struct drm_mode_fb_cmd2 const * const dumb_fb,
                            unsigned                        const width_mm,
                            unsigned                        const height_mm)
{
	intel_wakeref_t wakeref;

	int                        result   = -EINVAL;
	struct i915_gtt_view const view     = { .type = I915_GTT_VIEW_NORMAL };
	void   __iomem            *vaddr    = NULL;
	struct gem_dumb           *gem_dumb = NULL;
	struct drm_i915_private   *dev_priv = to_i915(dev->dev);
	struct drm_framebuffer    *fb       = drm_framebuffer_lookup(dev->dev,
	                                                             dev->file,
	                                                             dumb_fb->fb_id);

	if (!info || !fb) {
		printk("%s:%u error setting up info and fb\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (!dumb_gem(dev, fb, &gem_dumb) || !gem_dumb) {
		printk("%s:%u error looking up fb and vma\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (gem_dumb->vma) {
		intel_unpin_fb_vma(gem_dumb->vma, gem_dumb->flags);

		gem_dumb->vma   = NULL;
		gem_dumb->flags = 0;
	}

	wakeref = intel_runtime_pm_get(&dev_priv->runtime_pm);

	/* Pin the GGTT vma for our access via info->screen_base.
	 * This also validates that any existing fb inherited from the
	 * BIOS is suitable for own access.
	 */
	gem_dumb->vma = intel_pin_and_fence_fb_obj(fb, false /* phys_cursor */,
	                                           &view, false /* use fences */,
	                                           &gem_dumb->flags);

	if (IS_ERR(gem_dumb->vma)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(gem_dumb->vma);

		printk("%s:%u error setting vma %d\n", __func__, __LINE__, result);

		gem_dumb->vma = NULL;
		return result;
	}

	vaddr = i915_vma_pin_iomap(gem_dumb->vma);
	if (IS_ERR(vaddr)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(vaddr);
		printk("%s:%u error pin iomap %d\n", __func__, __LINE__, result);

		intel_unpin_fb_vma(gem_dumb->vma, gem_dumb->flags);
		gem_dumb->vma = NULL;

		return result;
	}

	/* fill framebuffer info for kernel_register_fb */
	info->screen_base        = vaddr;
	info->screen_size        = gem_dumb->vma->size;
	info->fix.line_length    = fb->pitches[0];
	info->var.bits_per_pixel = drm_format_info_bpp(fb->format, 0);

	intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

	kernel_register_fb(info, width_mm, height_mm);

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

		destroy_fb(dev, gem_dumb, dumb_fb);
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
