/*
 * \brief  Linux emulation Intel i915 parameter struct definition
 * \author Stefan Kalkowski
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <../drivers/gpu/drm/i915/i915_drv.h>

struct i915_params i915 = {
	.modeset = -1,
	.panel_ignore_lid = 1,
	.semaphores = -1,
	.lvds_channel_mode = 0,
	.panel_use_ssc = -1,
	.vbt_sdvo_panel_type = -1,
	.enable_rc6 = -1,
	.enable_fbc = -1,
	.enable_execlists = -1,
	.enable_hangcheck = false,
	.enable_ppgtt = -1,
	.enable_psr = 0,
	.preliminary_hw_support = true,
	.disable_power_well = -1,
	.enable_ips = 1,
	.fastboot = 0,
	.prefault_disable = 0,
	.load_detect_test = 0,
	.reset = true,
	.invert_brightness = 0,
	.disable_display = 0,
	.enable_cmd_parser = 1,
	.disable_vtd_wa = 0,
	.use_mmio_flip = 0,
	.mmio_debug = 0,
	.verbose_state_checks = 1,
	.nuclear_pageflip = 0,
	.edp_vswing = 0,
	.enable_guc_submission = false,
	.guc_log_level = -1,
};

