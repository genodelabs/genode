#
# Return Bender option that configures Bender's Intel HWP plugin
#
# \param --bender-intel-hwp-mode   Run the Intel HWP plugin of Bender in the
#                                  given mode. Valid argument values are
#                                     "off",
#                                     "performance",
#                                     "balanced", and
#                                     "power_saving"
#                                  The argument value defaults to
#                                  "performance".
#
proc bender_intel_hwp_mode_option { } {

	set opt [get_cmd_arg_first --bender-intel-hwp-mode "performance"]
	if {$opt == "off"} {
		return "intel_hwp_off"
	} elseif {$opt == "performance"} {
		return "intel_hwp_performance"
	} elseif {$opt == "balanced"} {
		return "intel_hwp_balanced"
	} elseif {$opt == "power_saving"} {
		return "intel_hwp_power_saving"
	} else {
		return "intel_hwp_performance"
	}
}

