source ${genode_dir}/repos/os/run/assert_nic.inc

#
# the test should be run isolated as it would disturb normal network operation
#
if {![have_include power_on/qemu]} {

	puts "Run script is not supported on this platform."
	exit 0
}
