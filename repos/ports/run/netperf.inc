#
# \brief  Test for using netperf
# \author Alexander Boettcher
# \author Martin Stein
# \date   2013-04-18
#

#
# To execute this run script on your Linux host you have to do some
# preparation:
#
# 1) Setup a TAP device:
#    ! export USER=[YOUR_USER_NAME]
#    ! export FLAVOR=[YOUR_TEST_FLAVOR] # e.g. 'lwip_bridge'
#    ! export TAP_DEV=tap0
#    ! sudo ip tuntap add dev $TAP_DEV mode tap user $USER
#    ! sudo ip address flush dev $TAP_DEV
#    ! sudo ip address add 10.0.2.1/24 brd 10.0.2.255 dev $TAP_DEV
#    ! sudo ip link set dev $TAP_DEV addr 02:00:00:ca:fe:01
#    ! sudo ip link set dev $TAP_DEV up
#
# 2) Now, start the test:
#    ! cd build/x86_64
#    ! make run/netperf_$FLAVOR KERNEL=linux BOARD=linux
#
# 3) Clean up your Linux when done testing:
#    ! sudo ip tuntap delete $TAP_DEV mode tap
#

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit 0
}

if {[have_board imx53_qsb_tz]} {
	puts "Run script is not supported on this platform."
	exit 0
}

proc unique_byte_per_platform { } {

	if {[expr ![have_include "power_on/qemu"]]} {
		if {[have_spec arm_v6]}  {
			if {[have_board rpi]} {
				if {[have_spec hw]}        { return 0; }
			}
		}
		if {[have_spec arm_v7a]} {
			if {[have_board imx53_qsb]} {
				if {[have_spec hw]}        { return 1; }
			}
			if {[have_board imx6q_sabrelite]} {
				if {[have_spec hw]}        { return 2; }
				if {[have_spec foc]}       { return 3; }
				if {[have_spec sel4]}      { return 4; }
			}
			if {[have_board imx7d_sabre]} {
				if {[have_spec hw]}        { return 5; }
				if {[have_spec foc]}       { return 6; }
				if {[have_spec sel4]}      { return 7; }
			}
		}
		if {[have_spec arm_v8a]} {
			if {[have_board imx8q_evk]} {
				if {[have_spec hw]}        { return 8; }
			}
		}
		if {[have_spec x86_32]} {
			if {[have_board linux]} {
				if {[have_spec linux]}     { return 9; }
			}
			if {[have_board pc]} {
				if {[have_spec foc]}       { return 10; }
				if {[have_spec sel4]}      { return 11; }
				if {[have_spec nova]}      { return 12; }
				if {[have_spec fiasco]}    { return 13; }
				if {[have_spec okl4]}      { return 14; }
				if {[have_spec pistachio]} { return 15; }
			}
		}
		if {[have_spec x86_64]} {
			if {[have_board linux]} {
				if {[have_spec linux]}     { return 16; }
			}
			if {[have_board pc]} {
				if {[have_spec hw]}        { return 17; }
				if {[have_spec foc]}       { return 18; }
				if {[have_spec sel4]}      { return 19; }
				if {[have_spec nova]}      { return 20; }
			}
		}
	}
	puts "Failed to determine unique byte for target."
	exit -1
}

proc server_data_port { } {

	return [expr 18000 + [unique_byte_per_platform]];
}

proc server_ctrl_port { } {

	return 12865;
}

set bridge_mac "02:02:02:02:16:00"

set version "2.6.0"

# sanity check that the right version is used
set wrong_version [catch {
	spawn netperf-$version -V
	expect {
		{Netperf version 2.6.0}  { }
		eof { return }
		timeout { return }
	}
}]

if {$wrong_version} {
	puts -nonewline "\nPlease compile a netperf client of version $version "
	puts "for your host system."
	puts "The sources are available in 'contrib/netperf-<hash>' (after you "
	puts "prepared the port by calling 'tool/ports/prepare_port netperf')."
	puts "Please name the binary netperf-$version\n"
	exit 1;
}

# netperf configuration
set packet_size 1024
set netperf_tests "TCP_STREAM TCP_MAERTS"

proc socket_fs_plugin {} {
	global use_lxip
	if { $use_lxip } { return lxip }
	return lwip
}

create_boot_directory

set packages "
	[depot_user]/src/[base_src]
	[depot_user]/pkg/[drivers_nic_pkg]
	[depot_user]/src/init
	[depot_user]/src/libc
	[depot_user]/src/posix
	[depot_user]/src/vfs
	[depot_user]/src/nic_router
"

append_if [expr !$use_lxip] packages " [depot_user]/src/vfs_lwip          "
append_if $use_lxip         packages " [depot_user]/src/vfs_lxip          "
append_if $use_nic_bridge   packages " [depot_user]/src/nic_bridge        "
append_if $use_usb_driver   packages " [depot_user]/src/platform_drv      "
append_if $use_wifi_driver  packages " [depot_user]/src/fs_rom            "
append_if $use_wifi_driver  packages " [depot_user]/src/vfs_import        "
append_if $use_wifi_driver  packages " [depot_user]/src/report_rom        "
append_if $use_wifi_driver  packages " [depot_user]/src/wifi_drv          "
append_if $use_wifi_driver  packages " [depot_user]/src/libcrypto         "
append_if $use_wifi_driver  packages " [depot_user]/src/libssl            "
append_if $use_wifi_driver  packages " [depot_user]/src/vfs_jitterentropy "
append_if $use_wifi_driver  packages " [depot_user]/raw/wifi_firmware     "

import_from_depot $packages

set build_components { app/netperf }

append_if $use_usb_driver build_components " drivers/usb_host "
append_if $use_usb_driver build_components " drivers/usb_net  "

build $build_components

#
# Generate config
#

set lx_ip_addr "10.0.2.55"

source ${genode_dir}/repos/base/run/platform_drv.inc
proc platform_drv_policy {} {
	global use_wifi_driver
	if { $use_wifi_driver } {
		return {<policy label_prefix="nic_drv"> <pci class="WIFI"/> </policy>}}
	return {<policy label_prefix="usb_host_drv"> <pci class="USB"/> </policy>}
}

set config {
<config verbose="yes" prio_levels="2">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start> }

if { $use_wifi_driver } {
	append_platform_drv_config
	append config {
	<start name="report_rom">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report" /> <service name="ROM" /> </provides>
		<config> <rom/> </config>
	</start>

	<start name="config_fs">
		<binary name="vfs"/>
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="File_system"/> </provides>
		<config>
			<vfs>
				<ram/>
				<import>
					<inline name="wifi_config">
<wifi_config connected_scan_interval="0" scan_interval="10" rfkill="no" verbose="no" verbose_state="no">}
append config "<network ssid=\"$wifi_ssid\" protection=\"WPA2\" passphrase=\"$wifi_psk\"/>"
append  config { </wifi_config>
					</inline>
				</import>
			</vfs>
			<policy label_prefix="config_rom" root="/"/>
		</config>
	</start>

	<start name="config_rom">
		<binary name="fs_rom"/>
		<resource name="RAM" quantum="4M"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="File_system"> <child name="config_fs" /> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic_drv" caps="300">
		<binary name="wifi_drv"/>
		<resource name="RAM" quantum="32M"/>
		<config mode="uplink_client">
			<libc stdout="/dev/null" stderr="/dev/log" rtc="/dev/rtc"/>
			<vfs>
				<dir name="dev">
					<log/> <null/>
					<jitterentropy name="random"/>
					<jitterentropy name="urandom"/>
					<inline name="rtc">2018-01-01 00:01</inline>
				</dir>
			</vfs>
		</config>
		<route>
			<service name="Rtc"> <any-child/> </service>
			<service name="File_system"> <child name="config_fs"/> </service>
			<service name="ROM" label="wifi_config"> <child name="config_rom" /> </service>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="Uplink"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

} elseif { $use_usb_driver } {

	append_platform_drv_config
	append config {

	<start name="usb_host_drv" caps="120"> }

	append config "<binary name=\"[usb_host_drv_binary]\"/>"
	append config {

		<resource name="RAM" quantum="12M"/>
		<provides> <service name="Usb"/> </provides>
		<config bios_handoff="yes"> }

	append_if [have_board arndale] config {

			<policy label_prefix="nic_drv" vendor_id="0x0b95" product_id="0x772a"/> }

	append_if [have_board rpi] config {

			<policy label_prefix="nic_drv" vendor_id="0x0424" product_id="0xec00"/> }

	append_if [have_spec x86] config {

			<policy label_prefix="nic_drv" vendor_id="0x0b95" product_id="0x1790"/> }

	append config {

		</config>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic_drv" caps="200">
		<binary name="usb_net_drv"/>
		<resource name="RAM" quantum="20M"/>
		<config mode="uplink_client" mac="02:00:00:00:01:01" />
		<route>
			<service name="Uplink"><child name="nic_router"/></service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

} else {

	append config {

	<start name="nic_drv" caps="1000" managing_system="yes">
		<resource name="RAM" quantum="32M"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Uplink"> <child name="nic_router"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start> }
}

append config {

	<start name="nic_router" caps="120">
		<resource name="RAM" quantum="5M"/>
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="yes"> }

if {$use_nic_bridge} { append config {

			<policy label_prefix="nic_bridge"       domain="server"/> }

} else { append config {

			<policy label_prefix="netserver_genode" domain="server"/> }
}
append config {

			<policy label_prefix="nic_drv"          domain="uplink"/>

			<domain name="uplink" }

append_if [have_spec linux] config "

			        interface=\"$lx_ip_addr/24\" gateway=\"10.0.2.1\""

append config {

			>
				<nat domain="server" tcp-ports="100" />
				<tcp-forward port="} [server_data_port] {" domain="server" to="10.0.3.2" />
				<tcp-forward port="} [server_ctrl_port] {" domain="server" to="10.0.3.2" />
			</domain>

			<domain name="server" interface="10.0.3.1/24" verbose_packets="no">
				<dhcp-server ip_first="10.0.3.2"
				             ip_last="10.0.3.2"
				             ip_lease_time_sec="600"/>
			</domain>

		</config>
	</start> }

append_if $use_nic_bridge config {

	<start name="nic_bridge">
		<resource name="RAM" quantum="5M"/>
		<provides><service name="Nic"/></provides>
		<config mac="} $bridge_mac {">
			<default-policy/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

append config {

	<start name="netserver_genode" caps="320" priority="-1">
	<binary name="netserver"/>
		<resource name="RAM" quantum="32M"/>
		<config>
			<arg value="netserver"/>
			<arg value="-D"/>
			<arg value="-4"/>
			<arg value="-f"/>
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc" socket="/socket"/>
			<vfs>
				<dir name="dev">
					<log/> <inline name="rtc">2018-01-01 00:01</inline>
				</dir>
				<dir name="socket">
					<} [socket_fs_plugin] { dhcp="yes"/>
				</dir>
			</vfs>
		</config>
		<route>
}
if { $use_nic_bridge } {

	append config {

			<service name="Nic"> <child name="nic_bridge"/> </service> }

} else {

	append config {

			<service name="Nic"> <child name="nic_router"/> </service> }
}
append config {

			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

</config> }

install_config $config

set boot_modules { netserver }

append_if $use_usb_driver boot_modules " [usb_host_drv_binary] "
append_if $use_usb_driver boot_modules " usb_net_drv  "

build_boot_image $boot_modules

#
# Execute test case
#

set ip_match_string "nic_router\\\] \\\[uplink\\\] dynamic IP config: interface (\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}).*\n"
set force_ports "-P [server_data_port],[server_data_port]"

if {[have_spec linux]} {
	run_genode_until {.*family AF_INET.*\n} 60
} else {
	run_genode_until $ip_match_string 60
}

set serial_id [output_spawn_id]

if [have_spec linux] {
	set ip_addr $lx_ip_addr
} else {
	regexp $ip_match_string $output all ip_addr
	puts ""
}

# give the TCP/IP stack some time to settle down
sleep 3

# start netperf client connecting to netperf server running native on Genode
foreach netperf_test $netperf_tests {
	puts "\n---------------------------- $netperf_test -----------------------"

	spawn netperf-$version -4 -H $ip_addr -P 1 -v 2 -t $netperf_test -c -C -- $packet_size $force_ports
	set netperf_id $spawn_id

	set spawn_id_list [list $netperf_id $serial_id]

	# reset output, so that we get on the second run not the result of the first
	set output ""
	run_genode_until "Segment" 120 $spawn_id_list

	# get throughput from netperf output
	set throughput [regexp -all -inline {([0-9]+[ ]+){3}[0-9]+\.[0-9]+[ ]+[0-9]+\.[0-9]+} $output]
	set throughput [regexp -all -inline {[0-9]+\.[0-9]+} $throughput]

	# calculate packets per second rate
	set all_bytes [regexp -all -inline {([0-9]+[ ]+){5}} $output]
	set all_bytes [lindex $all_bytes 0 4]

	set elapsed_time [regexp -all -inline {([0-9]+[ ]+){3}[0-9]+\.[0-9]+[ ]+} $output]
	set elapsed_time [lindex $elapsed_time 0 3]
	set packets_second [expr $all_bytes / $packet_size / $elapsed_time]

	puts "\ncalculation: overall bytes / size per packet / time = packets per second"
	puts -nonewline "             $all_bytes Bytes / $packet_size Bytes / $elapsed_time s = "
	puts "[format %8.0f $packets_second] packets/s\n"

	# format output parseable for post proccessing scripts
	puts -nonewline "! PERF: $netperf_test"
	if {$use_nic_bridge} { puts -nonewline "_bridge" }
	if {$use_usb_driver} { puts -nonewline "_xhci"   }
	puts "              [lindex $throughput 1] MBit/s ok"
}
