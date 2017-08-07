set xen_spawn_id    ""
set xen_args        [get_cmd_arg --xen-args ""]


#
# Enable run scripts to extend 'xen_arg' via 'append' without bothering
# about the required whitespace in front of the custom arguments.
#
append xen_args " "


proc xen_args { } {
	global xen_args
	return $xen_args
}


##
# Check whether Xen support is available
#
# XXX should by removed in favor of [have_include "exec/xen"]
#
proc is_xen_available { } {
	if {[have_spec linux]} { return false }

	if {![have_spec x86]} {
		puts stderr "skipping execution because platform is not supported by Xen"
		return false
	}
	return true
}
