#
# there are no nic driver packages for these targets
#
assert {![have_board rpi3]}
assert {![have_board imx53_qsb_tz]}

#
# these targets would require extra setup on the autopilot which is not desired
#
if {[have_cmd_switch --autopilot]} {
	assert {![have_board linux] &&
	        ![have_board zynq_qemu] &&
	        ![have_board virt_qemu_riscv]} \
		"Autopilot mode is not supported on this platform."
}
