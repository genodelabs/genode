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


enum { MAX_BRIGHTNESS  = 100, INVALID_BRIGHTNESS   = MAX_BRIGHTNESS + 1 };
enum { MAX_CONNECTORS  =  32, CONNECTOR_ID_MIRROR  = MAX_CONNECTORS - 1 };
enum { CAPTURE_RATE_MS =  10, ATTEMPTS_BEFORE_STOP = 7 };

       struct task_struct      * lx_user_task   = NULL;
static struct task_struct      * lx_update_task = NULL;
static struct drm_client_dev   * dev_client     = NULL;

static bool const verbose = false;

static struct state {
	struct drm_mode_create_dumb  fb_dumb;
	struct drm_mode_fb_cmd2      fb_cmd;
	struct drm_framebuffer     * fbs;
	struct i915_vma            * vma;
	unsigned long                vma_flags;
	uint8_t                      mode_id;
	uint8_t                      unchanged;
	bool                         mirrored;
	bool                         enabled;
} states [MAX_CONNECTORS] = { };


static int user_register_fb(struct drm_client_dev   const * const dev,
                            struct fb_info                * const info,
                            struct drm_framebuffer        * const fb,
                            struct i915_vma              ** vma,
                            unsigned long                 * vma_flags,
                            unsigned width_mm, unsigned height_mm);


static int check_resize_fb(struct drm_client_dev       * const dev,
                           struct drm_mode_create_dumb * const gem_dumb,
                           struct drm_mode_fb_cmd2     * const dumb_fb,
                           bool                        * const resized,
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


static inline bool fb_mirror_compatible(struct drm_display_mode const * const a,
                                        struct drm_display_mode const * const b)
{
	return a->vdisplay <= b->vdisplay && a->hdisplay <= b->hdisplay;
}


static int probe_and_apply_fbs(struct drm_client_dev *client, bool);


/*
 * Heuristic to calculate mixed resolution across all mirrored connectors
 */
static void mirror_heuristic(struct drm_device const * const dev,
                             struct drm_display_mode * const virtual,
                             struct drm_display_mode * const compound,
                             struct drm_display_mode * const min_mode)
{
	struct drm_connector          *connector        = NULL;
	struct drm_display_mode       *mode             = NULL;
	struct drm_connector_list_iter conn_iter;

	/* read Genode's config per connector */
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {
		struct drm_display_mode smallest  = { .hdisplay = ~0, .vdisplay = ~0 };
		struct drm_display_mode usable    = { };
		struct genode_mode      conf_mode = { };
		unsigned                mode_id   = 0;

		/* check for connector configuration on Genode side */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		if (!conf_mode.enabled || !conf_mode.mirror)
			continue;

		/* look for smallest possible mode or if a specific mode is specified */
		list_for_each_entry(mode, &connector->modes, head) {
			mode_id ++;

			if (!mode)
				continue;

			if (mode_larger(&smallest, mode)) {
				smallest.hdisplay = mode->hdisplay;
				smallest.vdisplay = mode->vdisplay;
			}

			/* maximal resolution enforcement */
			if (conf_mode.max_width && conf_mode.max_height) {
				if (conf_smaller_max_mode(&conf_mode, mode))
					continue;
			}

			if (!usable.hdisplay && !usable.vdisplay)
				usable = *mode;

			if (conf_mode.id == mode_id) {
				conf_mode.width  = mode->hdisplay;
				conf_mode.height = mode->vdisplay;
				break;
			}
		}

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
			virtual->hdisplay = conf_mode.force_width;
			virtual->vdisplay = conf_mode.force_height;
		}

		/* compound calculation */
		if (conf_mode.width && conf_mode.height) {
			if (conf_mode.width  > compound->hdisplay)
				compound->hdisplay = conf_mode.width;
			if (conf_mode.height > compound->vdisplay)
				compound->vdisplay = conf_mode.height;
		} else {
			if (usable.hdisplay && usable.vdisplay) {
				if (usable.hdisplay > compound->hdisplay)
					compound->hdisplay = usable.hdisplay;
				if (usable.vdisplay > compound->vdisplay)
					compound->vdisplay = usable.vdisplay;
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* no mode due to early bootup or all connectors are disabled by config */
	if (!min_mode->hdisplay || !min_mode->vdisplay)
		return;

	/* if no mirrored connectors are enabled, compound is the minimal mode */
	if (!compound->hdisplay || !compound->vdisplay)
		*compound = *min_mode;
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


static void destroy_fb(struct drm_client_dev       * const dev,
                       struct drm_mode_create_dumb * const gem_dumb,
                       struct drm_mode_fb_cmd2     * const dumb_fb)
{
	if (dumb_fb->fb_id) {
		int result = drm_mode_rmfb(dev->dev, dumb_fb->fb_id, dev->file);

		if (result) {
			drm_err(dev->dev, "%s: failed to remove framebuffer %d\n",
			        __func__, result);
		}
	}

	if (gem_dumb->handle) {
		int result = drm_mode_destroy_dumb(dev->dev, gem_dumb->handle, dev->file);

		if (result) {
			drm_err(dev->dev, "%s: failed to destroy framebuffer %d\n",
			        __func__, result);
		}
	}

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


static void destroy_fb_and_capture(struct drm_client_dev       * const dev,
                                   struct drm_connector  const * const connector,
                                   struct state                * const state)
{
	struct fb_info   info   = {};

	info.var.bits_per_pixel = 32;
	info.node               = connector->index;
	info.par                = connector->name;

	kernel_register_fb(&info, 0, 0);

	if (state->vma) {
		intel_unpin_fb_vma(state->vma,
		                   state->vma_flags);

		state->vma       = NULL;
		state->vma_flags = 0;
	}

	state->enabled   = false;
	state->unchanged = 0;

	destroy_fb(dev, &(state->fb_dumb), &(state->fb_cmd));
}


static void close_unused_captures(struct drm_client_dev * const dev)
{
	/* report disconnected connectors to close capture connections */
	struct drm_connector_list_iter   conn_iter;
	struct drm_connector           * connector     = NULL;
	bool                             mirror_in_use = false;

	drm_connector_list_iter_begin(dev->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {

		if (connector->index >= MAX_CONNECTORS)
			continue;

		if (connector->index == CONNECTOR_ID_MIRROR)
			continue;

		if (!states[connector->index].fbs)
			destroy_fb_and_capture(dev, connector, &states[connector->index]);
	}
	drm_connector_list_iter_end(&conn_iter);

	for (unsigned i = 0; i < MAX_CONNECTORS; i++) {
		if (i == CONNECTOR_ID_MIRROR)
			continue;

		if (!states[i].enabled || !states[i].mirrored)
			continue;

		mirror_in_use = true;
		break;
	}

	if (!mirror_in_use) {
		struct fb_info fb_info = {};

		fb_info.var.bits_per_pixel = 32;
		fb_info.node               = CONNECTOR_ID_MIRROR;
		fb_info.par                = "mirror_capture";

		kernel_register_fb(&fb_info, 0, 0);
	}
}


static struct drm_display_mode * best_mode(struct genode_mode      const * const conf,
                                           struct drm_connector    const * const connector,
                                           struct drm_display_mode const * const mirror_mode,
                                           bool                          * const no_match,
                                           unsigned                      * const id_mode)
{
	struct drm_display_mode * mode       = NULL;
	struct drm_display_mode * mode_match = NULL;
	unsigned                  mode_id    = 0;

	/* heuristics to find matching mode */
	list_for_each_entry(mode, &connector->modes, head) {
		mode_id ++;

		if (!mode)
			continue;

		/* mode larger mirrored_mode will fail */
		if (conf->mirror && !fb_mirror_compatible(mode, mirror_mode))
			continue;

		/* use mode id if configured and matches exactly */
		if (conf->id) {
			if (conf->id != mode_id)
				continue;

			mode_match = mode;
			break;
		}

		/* if invalid, mode is configured in second loop below */
		if (conf->width == 0 || conf->height == 0)
			break;

		/* no exact match by mode id -> try matching by size */
		if ((mode->hdisplay != conf->width) || (mode->vdisplay != conf->height))
			continue;

		/* take as default any mode with matching resolution */
		if (!mode_match) {
			mode_match = mode;
			continue;
		}

		/* replace matching mode iif hz matches exactly */
		if ((conf->hz != drm_mode_vrefresh(mode_match)) &&
		    (conf->hz == drm_mode_vrefresh(mode)))
			mode_match = mode;
	}

	mode_id = 0;

	/* second chance loop */
	list_for_each_entry(mode, &connector->modes, head) {

		mode_id ++;

		if (!mode)
			continue;

		/* use first mode for non mirrored connector in case of no match */
		if (!mode_match && !conf->mirror) {

			struct drm_display_mode max = { .hdisplay = conf->max_width,
			                                .vdisplay = conf->max_height };

			if (conf->max_width && conf->max_height) {
				if (conf_larger_mode(conf, &max))
					continue;
			}

			mode_match = mode;
		}

		/* no matching mode ? */
		if (!mode_match) {

			/* mode larger mirrored_mode will fail */
			if (conf->mirror && !fb_mirror_compatible(mode, mirror_mode))
				continue;

			/* use first smaller mode */
			mode_match = mode;

			if (conf->id)
				*no_match = true;
		}

		if (mode_match != mode)
			continue;

		*id_mode = mode_id;

		break;
	}

	return mode_match;
}


static void reconfigure(struct drm_client_dev * const dev)
{
	static struct drm_mode_create_dumb * gem_mirror    = NULL;
	static struct drm_mode_fb_cmd2     * mirror_fb_cmd = NULL;

	struct drm_connector_list_iter  conn_iter;
	struct drm_connector          * connector  = NULL;

	struct drm_display_mode    mirror_force    = {};
	struct drm_display_mode    mirror_compound = {};
	struct drm_display_mode    mirror_minimum  = {};
	struct drm_display_mode    mirror_fb       = {};

	struct {
		struct fb_info info;
		unsigned       width_mm;
		unsigned       height_mm;
		bool           report;
	} mirror = { { }, 0, 0, false };

	gem_mirror    = &states[CONNECTOR_ID_MIRROR].fb_dumb;
	mirror_fb_cmd = &states[CONNECTOR_ID_MIRROR].fb_cmd;

	if (!dev || !dev->dev || !gem_mirror || !mirror_fb_cmd)
		return;

	mirror_heuristic(dev->dev, &mirror_force, &mirror_compound,
	                 &mirror_minimum);

	if (!mirror_minimum.hdisplay || !mirror_minimum.vdisplay) {
		/* no valid modes on any connector on early boot */
		if (!mirror_fb_cmd->fb_id)
			return;

		/* valid connectors but all are disabled by config */
		mirror_minimum.hdisplay = mirror_fb_cmd->width;
		mirror_minimum.vdisplay = mirror_fb_cmd->height;
		mirror_compound         = mirror_minimum;
	}

	if (mode_larger(&mirror_compound, &mirror_minimum))
		mirror_fb = mirror_compound;
	else
		mirror_fb = mirror_minimum;

	{
		struct state * state_mirror = &states[CONNECTOR_ID_MIRROR];
		bool resized = false;

		int const err = check_resize_fb( dev,
		                                 gem_mirror,
		                                 mirror_fb_cmd,
		                                &resized,
		                                 mirror_fb.hdisplay,
		                                 mirror_fb.vdisplay);

		if (err) {
			printk("setting up mirrored framebuffer of %ux%u failed - error=%d\n",
			       mirror_fb.hdisplay, mirror_fb.vdisplay, err);

			return;
		}

		if (verbose) {
			printk("mirror: compound %ux%u force=%ux%u fb=%ux%u\n",
			       mirror_compound.hdisplay, mirror_compound.vdisplay,
			       mirror_force.hdisplay,    mirror_force.vdisplay,
			       mirror_fb.hdisplay,       mirror_fb.vdisplay);
		}

		/* if mirrored fb changed, drop reference and get new framebuffer */
		if (resized) {
			if (state_mirror->fbs)
				drm_framebuffer_put(state_mirror->fbs);

			state_mirror->fbs = drm_framebuffer_lookup(dev->dev, dev->file,
			                                           mirror_fb_cmd->fb_id);
		}

		mirror.info.var.xres         = mirror_fb.hdisplay;
		mirror.info.var.yres         = mirror_fb.vdisplay;
		mirror.info.var.xres_virtual = mirror_force.hdisplay ? : mirror_compound.hdisplay;
		mirror.info.var.yres_virtual = mirror_force.vdisplay ? : mirror_compound.vdisplay;
		mirror.info.node             = CONNECTOR_ID_MIRROR;
		mirror.info.par              = "mirror_capture";
	}

	/* without fb handle created by check_resize_fb we can't proceed */
	if (!mirror_fb_cmd->fb_id) {
		printk("%s:%u no mirror fb id\n", __func__, __LINE__);
		return;
	}

	drm_connector_list_iter_begin(dev_client->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {

		struct drm_display_mode * mode        = NULL;
		unsigned                  mode_id     = 0;
		bool                      no_match    = false;
		bool                      same_state  = false;
		bool                      resized     = false;
		int                       err         = -EINVAL;
		struct genode_mode        conf_mode   = {};
		struct fb_info            fb_info     = {};
		struct state            * state       = &states[connector->index];

		if (connector->index >= MAX_CONNECTORS) {
			printk("connector id too large %s %u\n",
			       connector->name, connector->index);
			continue;
		}

		/* read configuration of connector */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		/* drop old fb reference, taken again later */
		if (state->fbs) {
			drm_framebuffer_put(state->fbs);
			state->fbs = NULL;
		}

		/* lookup next mode */
		mode = best_mode(&conf_mode, connector, &mirror_fb, &no_match, &mode_id);

		/* reduce flickering if in same state */
		same_state = conf_mode.mirror  == state->mirrored &&
		             conf_mode.enabled == state->enabled  &&
		             mode_id           == state->mode_id;

		/* close capture on change of discrete -> mirror */
		if (!state->mirrored && conf_mode.mirror)
			destroy_fb_and_capture(dev, connector, state);

		state->mirrored = conf_mode.mirror;
		state->enabled  = conf_mode.enabled;
		state->mode_id  = mode_id;

		if (!mode)
			continue;

		/* prepare fb info for kernel_register_fb() evaluated by Genode side */
		if (conf_mode.mirror) {
			if (conf_mode.enabled)
				mirror.report = true;
			fb_info = mirror.info;
		} else {
			fb_info.var.xres         = mode->hdisplay;
			fb_info.var.yres         = mode->vdisplay;
			fb_info.var.xres_virtual = mode->hdisplay;
			fb_info.var.yres_virtual = mode->vdisplay;
			fb_info.node             = connector->index;
			fb_info.par              = connector->name;
		}

		/* diagnostics */
		if (verbose)
			printk("%10s: %s name='%9s' id=%u%s%s mode=%4ux%4u@%u%s fb=%4ux%4u%s",
			       connector->name ? connector->name : "unnamed",
			       conf_mode.enabled ? " enable" : "disable",
			       mode->name ? mode->name : "noname",
			       mode_id, mode_id < 10 ? " " : "",
			       conf_mode.mirror ? " mirror  " : " discrete",
			       mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode),
			       drm_mode_vrefresh(mode) < 100 ? " ": "",
			       fb_info.var.xres, fb_info.var.yres,
			       (no_match) ? "" : "\n");

		if (verbose && no_match)
			printk(" - no mode match: %ux%u\n",
			       conf_mode.width,
			       conf_mode.height);

		if (!conf_mode.enabled)
			continue;

		/* set brightness */
		if (conf_mode.brightness <= MAX_BRIGHTNESS) {
			drm_modeset_lock(&dev->dev->mode_config.connection_mutex, NULL);
			set_brightness(conf_mode.enabled ? conf_mode.brightness : 0,
			               connector);
			drm_modeset_unlock(&dev->dev->mode_config.connection_mutex);
		}

		if (conf_mode.mirror) {
			/* get new fb reference for mirrored fb */
			state->fbs = drm_framebuffer_lookup(dev->dev, dev->file,
			                                    mirror_fb_cmd->fb_id);
			continue;
		}

		/* discrete case handling */

		err = check_resize_fb(dev, &state->fb_dumb, &state->fb_cmd,
		                      &resized, mode->hdisplay, mode->vdisplay);
		if (err) {
			printk("setting up framebuffer of %ux%u failed - error=%d\n",
			       mode->hdisplay, mode->vdisplay, err);
		}

		/* get new fb reference after check_resize_fb */
		state->fbs = drm_framebuffer_lookup(dev->dev, dev->file,
		                                    state->fb_cmd.fb_id);

		if (verbose)
			printk("%s:%u %s %s %s\n", __func__, __LINE__, connector->name,
			       same_state ? " same state " : " different state",
			       resized    ? " resized " : "not resized");

		if (state->fbs && (!same_state || resized)) {
			unsigned width_mm  = mode->width_mm  ? : connector->display_info.width_mm;
			unsigned height_mm = mode->height_mm ? : connector->display_info.height_mm;

			int err = user_register_fb(dev, &fb_info, state->fbs,
			                           &state->vma, &state->vma_flags,
			                           width_mm, height_mm);

			if (err == -ENOSPC) {
				if (state->fbs) {
					drm_framebuffer_put(state->fbs);
					state->fbs = NULL;
				}
				destroy_fb_and_capture(dev, connector, state);
			}
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (mirror.report) {
		user_register_fb(dev, &mirror.info,
		                  states[CONNECTOR_ID_MIRROR].fbs,
		                 &states[CONNECTOR_ID_MIRROR].vma,
		                 &states[CONNECTOR_ID_MIRROR].vma_flags,
		                 mirror.width_mm, mirror.height_mm);
	}

	close_unused_captures(dev);

	return;
}


static int do_action_loop(void * data)
{
	int status_last_action = !ACTION_FAILED;

	while (true) {

		int const action = lx_emul_i915_action_to_process(status_last_action);

		switch (action) {
		case ACTION_DETECT_MODES:
			/* probe new modes of connectors and apply very same previous fbs */
			status_last_action = probe_and_apply_fbs(dev_client, true)
			                   ? ACTION_FAILED : !ACTION_FAILED;
			break;
		case ACTION_CONFIGURE:
			/* re-read Genode configuration and resize fbs depending on config */
			reconfigure(dev_client);

			/*
			 * Apply current fbs to connectors. It may fail when
			 * the reconfigure decision is outdated. By invoking the Linux
			 * probe code we may "see" already new hardware state.
			 */
			status_last_action = probe_and_apply_fbs(dev_client, false)
			                   ? ACTION_FAILED : !ACTION_FAILED;

			break;
		default:
			lx_emul_task_schedule(true /* block task */);
			break;
		}
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


void lx_emul_i915_wakeup(unsigned const connector_id)
{
	bool const valid_id  = connector_id < MAX_CONNECTORS;

	if (!valid_id) {
		printk("%s: connector id invalid %d\n", __func__, connector_id);
		return;
	}

	states[connector_id].unchanged = 0;

	/* wake potential sleeping update task */
	lx_emul_task_unblock(lx_update_task);
}


static int update_content(void *)
{
	while (true) {
		struct drm_connector_list_iter   conn_iter;
		struct drm_connector           * connector  = NULL;
		struct drm_device const        * dev        = dev_client->dev;
		bool                             block_task = true;
		bool                             mirror_run = false;

		drm_connector_list_iter_begin(dev, &conn_iter);
		drm_client_for_each_connector_iter(connector, &conn_iter) {

			bool       may_sleep = false;
			unsigned   index     = connector->index;

			if (connector->status != connector_status_connected)
				continue;

			if (connector->index >= MAX_CONNECTORS) {
				printk("%s: connector id invalid %d\n", __func__, index);
				index = CONNECTOR_ID_MIRROR; /* should never happen case */
			}

			if (!states[index].enabled)
				continue;

			if (states[index].mirrored) {
				if (mirror_run)
					continue;

				mirror_run = true;
				index = CONNECTOR_ID_MIRROR;
			}

			states[index].unchanged ++;

			may_sleep = states[index].unchanged >= ATTEMPTS_BEFORE_STOP;

			if (!lx_emul_i915_blit(index, may_sleep)) {
				if (!may_sleep)
					block_task = false;

				continue;
			}

			block_task = false;

			states[index].unchanged = 0;

			if (states[index].fbs)
				mark_framebuffer_dirty(states[index].fbs);
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
	int pid  = kernel_thread(do_action_loop, NULL, "lx_user",
	                         CLONE_FS | CLONE_FILES);
	int pid2 = kernel_thread(update_content, NULL, "lx_update",
	                         CLONE_FS | CLONE_FILES);

	lx_user_task   = find_task_by_pid_ns(pid , NULL);
	lx_update_task = find_task_by_pid_ns(pid2, NULL);
}


static void _report_connectors(void * genode_data, bool const discrete)
{
	struct drm_connector_list_iter   conn_iter;
	struct drm_connector           * connector = NULL;

	drm_connector_list_iter_begin(dev_client->dev, &conn_iter);
	drm_client_for_each_connector_iter(connector, &conn_iter) {

		bool const valid_fb = connector->index < MAX_CONNECTORS
		                    ? states[connector->index].fbs : false;

		struct genode_mode conf_mode = {};

		/* read configuration for connector */
		lx_emul_i915_connector_config(connector->name, &conf_mode);

		if ((discrete && conf_mode.mirror) || (!discrete && !conf_mode.mirror))
			continue;

		lx_emul_i915_report_connector(connector, genode_data,
		                              connector->name,
		                              connector->status != connector_status_disconnected,
		                              valid_fb,
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
	struct drm_connector    * connector   = lx_data;
	struct drm_display_mode * mode        = NULL;
	struct drm_display_mode * prev_mode   = NULL;
	unsigned                  mode_id     = 0;
	bool                      quirk_inuse = false;
	struct state            * state       = &states[connector->index];
	struct genode_mode        conf_mode   = { };

	if (connector->index >= MAX_CONNECTORS)
		return;

	lx_emul_i915_connector_config(connector->name, &conf_mode);

	/* no fb and conf_mode.enabled is a temporary inconsistent state */
	quirk_inuse = conf_mode.enabled && !state->fbs;

	list_for_each_entry(mode, &connector->modes, head) {
		bool skip = false;

		mode_id ++;

		if (!mode)
			continue;

		/* skip consecutive similar modes */
		if (prev_mode) {
			static_assert(sizeof(mode->name) == DRM_DISPLAY_MODE_LEN);
			skip = (mode->hdisplay == prev_mode->hdisplay) &&
			       (mode->vdisplay == prev_mode->vdisplay) &&
			       (drm_mode_vrefresh(mode) == drm_mode_vrefresh(prev_mode)) &&
			       !strncmp(mode->name, prev_mode->name, DRM_DISPLAY_MODE_LEN);
		}

		prev_mode = mode;

		{
			bool const max_mode = conf_mode.max_width && conf_mode.max_height;

			struct genode_mode config_report = {
				.width     = mode->hdisplay,
				.height    = mode->vdisplay,
				.width_mm  = mode->width_mm,
				.height_mm = mode->height_mm,
				.preferred = mode->type & (DRM_MODE_TYPE_PREFERRED |
				                           DRM_MODE_TYPE_DEFAULT),
				.inuse     = !quirk_inuse && state->mode_id == mode_id,
				.mirror    = state->mirrored,
				.hz        = drm_mode_vrefresh(mode),
				.id        = mode_id,
				.enabled   = !max_mode ||
				             !conf_smaller_max_mode(&conf_mode, mode)
			};

			/*
			 * Report first usable mode as used mode in the quirk state to
			 * avoid sending a mode list with no used mode at all, which
			 * external configuration components may trigger to disable the
			 * connector.
			 */
			if (quirk_inuse && config_report.enabled) {
				config_report.inuse = true;
				quirk_inuse         = false;
			}

			/* skip similar mode and if it is not the used one */
			if (skip && !config_report.inuse)
				continue;

			{
				static_assert(sizeof(config_report.name) == DRM_DISPLAY_MODE_LEN);
				memcpy(config_report.name, mode->name, sizeof(config_report.name));
			}

			lx_emul_i915_report_modes(genode_data, &config_report);
		}
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


static int fb_client_hotplug(struct drm_client_dev *)
{
	/* notify Genode side */
	lx_emul_i915_hotplug_connector();

	return 0;
}


static int probe_and_apply_fbs(struct drm_client_dev *client, bool const detect)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_mode_set *modeset = NULL;
	int result = -EINVAL;

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

	/* (re-)assign framebuffers to enabled connectors */
	DRM_MODESET_LOCK_ALL_BEGIN(client->dev, ctx,
	                           DRM_MODESET_ACQUIRE_INTERRUPTIBLE,
	                           result);

	/*
	 * (Re-)assign framebuffers to modeset (lost due to modeset_probe)
	 * and commit the change.
	 */
	mutex_lock(&client->modeset_mutex);
	drm_client_for_each_modeset(modeset, client) {
		struct drm_connector * connector = NULL;

		if (!modeset)
			continue;

		if (!modeset->num_connectors || !modeset->connectors || !*modeset->connectors)
			continue;

		/* set connector */
		connector = *modeset->connectors;

		if (verbose)
			printk("%s:%u %s fb=%px i=%u fbs[i]=%px %s\n",
			       __func__, __LINE__,
			       connector->name, modeset->fb,
			       connector->index,
			       connector->index < MAX_CONNECTORS ?
			       states[connector->index].fbs : NULL,
			       detect ? " - detect run" : " - configure run");

		modeset->fb = connector->index < MAX_CONNECTORS
		            ? states[connector->index].fbs
		            : NULL;

		if (!modeset->fb) {
			/*
			 * If no fb is available for the (new) connector, we have to
			 * explicitly revert structures prepared by
			 * drm_client_modeset_probe so that drm_client_modeset_commit
			 * will accept the modeset.
			 */
			for (unsigned i = 0; i < modeset->num_connectors; i++) {
				drm_connector_put(modeset->connectors[i]);
				modeset->connectors[i] = NULL;
			}

			modeset->num_connectors = 0;

			if (modeset->mode) {
				kfree(modeset->mode);
				modeset->mode = NULL;
			}
		} else {
			struct drm_display_mode * mode    = NULL;
			unsigned                  mode_id = 0;

			/* check and select right mode in modeset as configured by state[] */
			list_for_each_entry(mode, &connector->modes, head) {
				mode_id ++;

				if (!mode)
					continue;

				if (states[connector->index].mode_id != mode_id)
					continue;

				if (drm_mode_equal(mode, modeset->mode))
					break;

				if (modeset->mode)
					kfree(modeset->mode);

				modeset->mode = drm_mode_duplicate(client->dev, mode);

				break;
			}
		}
	}
	mutex_unlock(&client->modeset_mutex);
	DRM_MODESET_LOCK_ALL_END(client->dev, ctx, result);

	/* triggers also disablement of encoders attached to disconnected ports */
	result = drm_client_modeset_commit(client);

	if (result) {
		printk("%s: error on modeset commit %d%s\n", __func__, result,
		       (result == -ENOSPC) ? " - ENOSPC" : " - unknown error");
	}

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


static int user_register_fb(struct drm_client_dev const * const dev,
                            struct fb_info              * const info,
                            struct drm_framebuffer      * const fb,
                            struct i915_vma            ** const vma,
                            unsigned long               * const vma_flags,
                            unsigned                      const width_mm,
                            unsigned                      const height_mm)
{
	intel_wakeref_t wakeref;

	int                        result   = -EINVAL;
	struct i915_gtt_view const view     = { .type = I915_GTT_VIEW_NORMAL };
	void   __iomem            *vaddr    = NULL;
	struct drm_i915_private   *dev_priv = to_i915(dev->dev);

	if (!info || !fb || !dev_priv || !vma || !vma_flags) {
		printk("%s:%u error setting up info and fb\n", __func__, __LINE__);
		return -ENODEV;
	}

	if (*vma) {
		intel_unpin_fb_vma(*vma, *vma_flags);

		*vma       = NULL;
		*vma_flags = 0;
	}

	wakeref = intel_runtime_pm_get(&dev_priv->runtime_pm);

	/* Pin the GGTT vma for our access via info->screen_base.
	 * This also validates that any existing fb inherited from the
	 * BIOS is suitable for own access.
	 */
	*vma = intel_pin_and_fence_fb_obj(   fb, false /* phys_cursor */,
	                                  &view, false /* use fences */,
	                                   vma_flags);

	if (IS_ERR(*vma)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(*vma);

		printk("%s:%u error setting vma %d\n", __func__, __LINE__, result);

		*vma = NULL;
		*vma_flags = 0;

		return result;
	}

	if (!i915_vma_is_map_and_fenceable(*vma)) {
		printk("%s: framebuffer not mappable in aperture -> destroying framebuffer\n",
		       (info && info->par) ? (char *)info->par : "unknown");

		intel_unpin_fb_vma(*vma, *vma_flags);

		*vma       = NULL;
		*vma_flags = 0;

		return -ENOSPC;
	}

	vaddr = i915_vma_pin_iomap(*vma);

	if (IS_ERR(vaddr)) {
		intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

		result = PTR_ERR(vaddr);
		printk("%s:%u error pin iomap %d\n", __func__, __LINE__, result);

		intel_unpin_fb_vma(*vma, *vma_flags);

		*vma       = NULL;
		*vma_flags = 0;

		return result;
	}

	/* fill framebuffer info for kernel_register_fb */
	info->screen_base        = vaddr;
	info->screen_size        = (*vma)->size;
	info->fix.line_length    = fb->pitches[0];
	info->var.bits_per_pixel = drm_format_info_bpp(fb->format, 0);

	intel_runtime_pm_put(&dev_priv->runtime_pm, wakeref);

	kernel_register_fb(info, width_mm, height_mm);

	return 0;
}


static int check_resize_fb(struct drm_client_dev       * const dev,
                           struct drm_mode_create_dumb * const gem_dumb,
                           struct drm_mode_fb_cmd2     * const dumb_fb,
                           bool                        * const resized,
                           unsigned                      const width,
                           unsigned                      const height)
{
	int result = -EINVAL;

	/* paranoia */
	if (!dev || !dev->dev || !dev->file || !gem_dumb || !dumb_fb || !resized)
		return -ENODEV;

	*resized = false;

	/* if requested size is smaller, free up current dumb buffer */
	if (gem_dumb->width && gem_dumb->height &&
	    (gem_dumb->width < width || gem_dumb->height < height)) {

		destroy_fb(dev, gem_dumb, dumb_fb);

		*resized = true;
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

		*resized = true;
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

		*resized = true;
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
