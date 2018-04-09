/*
 * \brief  Linux emulation Intel i915 parameter struct definition
 * \author Stefan Kalkowski
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <../drivers/gpu/drm/i915/i915_drv.h>
struct i915_params i915_modparams = {
	.vbt_firmware = NULL,
	.modeset = -1,
	.panel_ignore_lid = 1,
	.lvds_channel_mode = 0,
	.panel_use_ssc = -1,
	.vbt_sdvo_panel_type = -1,
	.enable_dc  = -1,
	.enable_fbc = -1,
	.enable_ppgtt = 0,
	.enable_psr = 0,
	.disable_power_well = -1,
	.enable_ips = true,
	.invert_brightness = 0,
	.guc_log_level = -1,
	.guc_firmware_path = NULL,
	.huc_firmware_path = NULL,
	.mmio_debug = 0,
	.edp_vswing = 0,
	.enable_guc = 0,
	.reset = 0,
	.inject_load_failure = 0,
	.alpha_support = IS_ENABLED(CONFIG_DRM_I915_ALPHA_SUPPORT),
	.enable_cmd_parser = true,
	.enable_hangcheck = false,
	.fastboot = false,
	.prefault_disable = false,
	.load_detect_test = false,
	.force_reset_modeset_test = false,
	.error_capture            = true,
	.disable_display          = false,
	.verbose_state_checks     = true,
	.nuclear_pageflip         = false,
	.enable_dp_mst            = false,
	.enable_dpcd_backlight    = false,
	.enable_gvt               = false
};
