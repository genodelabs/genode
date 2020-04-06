set qemu_spawn_id    ""
set qemu_args        [get_cmd_arg --qemu-args ""]


#
# Enable run scripts to extend 'qemu_arg' via 'append' without bothering
# about the required whitespace in front of the custom arguments.
#
append qemu_args " "


proc qemu_args { } {
	global qemu_args
	return $qemu_args
}


##
# Check whether Qemu support is available
#
# XXX should by removed in favor of [have_include "exec/qemu"]
#
proc is_qemu_available { } {
	if {[expr [have_spec linux] && {"[board]"} == {"linux"}]} { return false }

	if {[have_spec rpi]} {
		puts stderr "skipping execution because platform is not supported by qemu"
		return false
	}
	return true
}
