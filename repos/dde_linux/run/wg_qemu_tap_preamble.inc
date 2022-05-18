#
# Preamble for WireGuard tests that run in Qemu and use a Qemu netdev TAP
# device to create a tunnel between the WireGuard device in the guest system
# and a Wireguard device in the host system. This script checks that the
# target platform is supported, installs the correct Qemu arguments, sets up
# the host WireGuard device, and ensures that the host WireGuard setup is
# undone whenever the test terminates.
#

if {[expr ![have_spec arm_64] && ![have_spec x86_64]]} {
	puts "Run script is not supported on this platform."
	exit 0
}

if {[expr [have_spec arm_64] && ![have_board virt_qemu]]} {
	puts "Run script is not supported on this platform."
	exit 0
}

if {[expr ![have_include power_on/qemu]]} {
	puts "Test requires Qemu."
	exit 0
}

append wg_board_qemu_args " -m 512 "
append wg_board_qemu_args " -nographic "
append wg_board_qemu_args " -netdev tap,id=net0,ifname=tap0,script=no,downscript=no "
append wg_board_qemu_args " -smp 4 "

if {[have_board virt_qemu]} {

	append wg_board_qemu_args " -global virtio-mmio.force-legacy=false "
	append wg_board_qemu_args " -device virtio-net-device,bus=virtio-mmio-bus.0,netdev=net0 "
	append wg_board_qemu_args " -M virt,virtualization=true,gic-version=3 "
	append wg_board_qemu_args " -cpu cortex-a53 "

} elseif {[have_spec x86_64]} {

	append wg_board_qemu_args " -machine q35 "
	append wg_board_qemu_args " -net nic,model=e1000,netdev=net0 "

} else {

	puts "Run script is not supported on this platform."
	exit 0
}

proc board_qemu_args { } {
	global wg_board_qemu_args
	return $wg_board_qemu_args
}

proc set_up_host_wg {} {

	puts "Set up host WireGuard"

	exec sudo rm -f udhcpd.conf udhcpd.leases
	exec sudo ip tuntap del dev tap0 mode tap

	set fd [open udhcpd.conf w]
	puts $fd "interface tap0"
	puts $fd "start 10.0.2.3"
	puts $fd "end   10.0.2.4"
	puts $fd "max_leases 2"
	puts $fd "lease_file udhcpd.leases"
	puts $fd "pidfile udhcpd.pid"
	puts $fd "option dns 10.0.2.1 10.0.2.2"
	puts $fd "option subnet 255.255.255.0"
	puts $fd "option router 10.0.2.1"
	close $fd

	set fd [open udhcpd.leases w]
	close $fd

	exec sudo ip tuntap add dev tap0 mode tap user $::tcl_platform(user)
	exec sudo ip address flush dev tap0
	exec sudo ip address add 10.0.2.1/24 dev tap0
	exec sudo ip link set tap0 up

	exec sudo udhcpd udhcpd.conf

	exec echo "0CtU34qsl97IGiYKSO4tMaF/SJvy04zzeQkhZEbZSk0=" > wg_private_key
	exec chmod 700 wg_private_key
	exec sudo ip link add wg0 type wireguard
	exec sudo ip addr add 10.0.9.1/24 dev wg0
	exec sudo wg set wg0 private-key wg_private_key listen-port 49001
	exec sudo ip link set wg0 up
	exec sudo wg set wg0 peer "GrvyALPZ3PQ2AWM+ovxJqnxSqKpmTyqUui5jH+C8I0E=" allowed-ips 10.0.9.2/32 endpoint 10.0.2.3:49002
}

proc undo_host_wg_setup {} {

	puts "Undo host WireGuard setup"
	exec sudo pkill -F udhcpd.pid
	exec sudo ip link set wg0 down
	exec sudo ip link delete dev wg0
	exec sudo ip link set tap0 down
	exec sudo ip address flush dev tap0
	exec sudo ip link delete tap0
	exec sudo rm -rf wg_private_key udhcpd.conf udhcpd.leases udhcpd.pid
}

rename exit run_tool_exit
proc exit {{status 0}} {

	undo_host_wg_setup
	run_tool_exit $status
}

set_up_host_wg
