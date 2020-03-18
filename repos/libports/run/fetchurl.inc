#
# \brief  Test of fetchurl
# \author Emery Hemingway
# \date   2016-06-05
#

if {[have_spec linux] || [have_spec rpi3] ||
    [expr [have_spec imx53] && [have_spec trustzone]]} {
	puts "Run script does not support this platform."
	exit 0
}

proc socket_fs_plugin {} {
	global use_lxip
	if { $use_lxip } { return lxip }
	return lwip
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg] \
                  [depot_user]/src/curl \
                  [depot_user]/src/fetchurl \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/libcrypto \
                  [depot_user]/src/libssh \
                  [depot_user]/src/libssl \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_[socket_fs_plugin] \
                  [depot_user]/src/zlib

install_config {
<config>
	<parent-provides>
		<service name="CPU"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="IRQ"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RAM"/>
		<service name="RM"/>
		<service name="ROM"/>
	</parent-provides>
	<default caps="100"/>
	<default-route>
		<service name="Report"> <child name="report_rom"/> </service>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="drivers" caps="1000">
		<resource name="RAM" quantum="32M" constrain_phys="yes"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<provides> <service name="Nic"/> </provides>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="ROM"/> <service name="Report"/> </provides>
		<config verbose="yes"/>
	</start>

	<start name="fetchurl" caps="500">
		<resource name="RAM" quantum="32M"/>
		<config>
			<report progress="yes"/>
			<vfs>
				<dir name="dev">
					<log/> <null/> <inline name="rtc">2000-01-01 00:00</inline>
					<inline name="random">01234567890123456789</inline>
				</dir>
				<dir name="socket"> <} [socket_fs_plugin] { dhcp="yes"/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc" socket="/socket"/>
			<fetch url="https://genode.org/about/LICENSE" path="/dev/log" retry="3"/>
		</config>
	</start>
</config>
}

build_boot_image {}

proc qemu_nic_model {} {
	if [have_spec x86]         { return e1000 }
	if [have_spec lan9118]     { return lan9118 }
	if [have_spec zynq]        { return cadence_gem }
	return nic_model_missing
}

append qemu_args " -nographic "
append qemu_args " -netdev user,id=net0 "
append qemu_args " -net nic,model=[qemu_nic_model],netdev=net0 "

## Uncomment to dump network traffic to file
# append qemu_args " -object filter-dump,id=net0,netdev=net0,file=[run_dir].pcap"

run_genode_until {child "fetchurl" exited with exit value 0} 120
