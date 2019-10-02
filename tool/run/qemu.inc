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

proc qemu_nic_model {} {
	if [have_spec lan9118]     { return lan9118 }
	if [have_spec zynq]        { return cadence_gem }
	if [have_spec x86]         { return e1000 }
	return nic_model_missing
}

proc is_virtio_pci_platform {} {
	return [expr [have_spec virtio] && [have_spec pci]]
}

proc append_qemu_nic_args { { extra_netdev_args "" } } {
	global qemu_args
	append qemu_args " -netdev user,id=net0"

	if { $extra_netdev_args ne "" } {
		append qemu_args ",$extra_netdev_args"
	}

	if {[have_spec virtio]} {
		if {[is_virtio_pci_platform]} {
			append qemu_args " -device virtio-net-pci,netdev=net0"
		} else {
			append qemu_args " -device virtio-net-device,netdev=net0"
		}
	} else {
		append qemu_args " -net nic,model=[qemu_nic_model],netdev=net0 "
	}
}


##
# Check whether Qemu support is available
#
# XXX should by removed in favor of [have_include "exec/qemu"]
#
proc is_qemu_available { } {
	if {[expr [have_spec linux] && {"[board]"} == {"linux"}]} { return false }

	if {[have_spec arndale]
	 || [have_spec rpi]} {
		puts stderr "skipping execution because platform is not supported by qemu"
		return false
	}
	return true
}
