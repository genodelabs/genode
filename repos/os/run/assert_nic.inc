#
# there are no nic driver packages for these targets
#
if {[have_board rpi3] ||
    [have_board imx53_qsb_tz]} {

	puts "Run script is not supported on this platform."
	exit 0
}

#
# these targets would require extra setup on the autopilot which is not desired
#
if {[get_cmd_switch --autopilot] && ([have_spec linux] ||
                                     [have_board zynq_qemu] ||
                                     [have_board virt_qemu_riscv])} {

	puts "Autopilot mode is not supported on this platform."
	exit 0
}
