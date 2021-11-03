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

proc append_qemu_nic_args { { extra_netdev_args "" } } {
	global qemu_args
	append qemu_args " -netdev user,id=net0"

	if { $extra_netdev_args ne "" } {
		append qemu_args ",$extra_netdev_args"
	}
}


##
# Check whether Qemu support is available
#
proc is_qemu_available { } {
	set qemu_args_file [file join "board" [board] "qemu_args"]

	set repo [repository_contains $qemu_args_file]

	if {$repo == ""} {
		puts stderr "skipping execution because platform is not supported by qemu"
		return false
	}
	return true
}
