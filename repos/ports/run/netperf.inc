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

if {[have_board imx53_qsb_tz] || [have_board rpi]} {
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
			if {[have_board zynq_usrp_e31x]} {
				if {[have_spec hw]}        { return 8; }
			}
		}
		if {[have_spec arm_v8a]} {
			if {[have_board imx8q_evk]} {
				if {[have_spec hw]}        { return 20; }
			}
		}
		if {[have_spec x86_32]} {
			if {[have_board linux]} {
				if {[have_spec linux]}     { return 40; }
			}
			if {[have_board pc]} {
				if {[have_spec foc]}       { return 41; }
				if {[have_spec sel4]}      { return 42; }
				if {[have_spec nova]}      { return 43; }
				if {[have_spec fiasco]}    { return 44; }
				if {[have_spec okl4]}      { return 45; }
				if {[have_spec pistachio]} { return 46; }
			}
		}
		if {[have_spec x86_64]} {
			if {[have_board linux]} {
				if {[have_spec linux]}     { return 60; }
			}
			if {[have_board pc]} {
				if {[have_spec hw]}        { return 61; }
				if {[have_spec foc]}       { return 62; }
				if {[have_spec sel4]}      { return 63; }
				if {[have_spec nova]}      { return 64; }
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
set netperf_tests "TCP_STREAM TCP_MAERTS"

proc socket_fs_plugin {} {
	global use_lxip
	if { $use_lxip } { return lxip }
	return lwip
}

create_boot_directory

set packages "
	[depot_user]/src/[base_src]
	[depot_user]/src/init
	[depot_user]/src/libc
	[depot_user]/src/posix
	[depot_user]/src/report_rom
	[depot_user]/src/vfs
	[depot_user]/src/nic_router
"
set use_nic_driver [expr !$use_usb_driver && !$use_wifi_driver]
append_if $use_nic_driver   packages " [depot_user]/pkg/[drivers_nic_pkg]     "
append_if [expr !$use_lxip] packages " [depot_user]/src/vfs_lwip              "
append_if $use_lxip         packages " [depot_user]/src/vfs_lxip              "
append_if $use_nic_bridge   packages " [depot_user]/src/nic_bridge            "
append_if $use_usb_driver   packages " [depot_user]/pkg/test_usb_host-[board] "
append_if $use_usb_driver   packages " [depot_user]/src/usb_net               "
append_if $use_wifi_driver  packages " [depot_user]/src/fs_rom                "
append_if $use_wifi_driver  packages " [depot_user]/src/vfs_import            "
append_if $use_wifi_driver  packages " [depot_user]/pkg/pc_wifi               "
append_if $use_wifi_driver  packages " [depot_user]/src/acpi                  "
append_if $use_wifi_driver  packages " [depot_user]/src/pci_decode            "
append_if $use_wifi_driver  packages " [depot_user]/src/platform              "

import_from_depot $packages

build { app/netperf }

#
# Generate config
#

set lx_ip_addr "10.0.2.55"

set config {
<config verbose="yes" prio_levels="4">
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
		<resource name="CPU" quantum="10"/>
		<provides> <service name="Timer"/> </provides>
	</start> }

if { $use_wifi_driver } {
	append config {
	<start name="report_rom" caps="100" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="ROM" />
			<service name="Report" />
		</provides>
		<config>
			<policy label="pci_decode -> system" report="acpi -> acpi"/>
			<policy label="platform -> devices"  report="pci_decode -> devices"/>
		</config>
	</start>

	<start name="acpi" caps="250" priority="-1">
		<resource name="RAM" quantum="4M"/>
	</start>

	<start name="pci_decode" caps="350" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<route>
			<service name="ROM" label="system"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="platform" caps="100" managing_system="yes" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Platform"/> </provides>
		<route>
			<service name="ROM" label="devices"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
		<config>
			<policy label_prefix="nic" info="yes"> <pci class="WIFI"/> </policy>
		</config>
	</start>

	<start name="config_fs" priority="-1">
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

	<start name="config_rom" priority="-1">
		<binary name="fs_rom"/>
		<resource name="RAM" quantum="4M"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="File_system"> <child name="config_fs" /> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic" caps="340" priority="-1">
		<binary name="wifi"/>
		<resource name="RAM" quantum="32M"/>
		<config>
			<libc stdout="/dev/null" stderr="/dev/log" rtc="/dev/rtc"/>
			<vfs>
				<dir name="dev">
					<log/> <null/>
					<jitterentropy name="random"/>
					<jitterentropy name="urandom"/>
					<inline name="rtc">2018-01-01 00:01</inline>
				</dir>
				<dir name="firmware">
					<tar name="wifi_firmware.tar"/>
				</dir>
			</vfs>
		</config>
		<route>
			<service name="Rtc"> <any-child/> </service>
			<service name="File_system"> <child name="config_fs"/> </service>
			<service name="ROM" label="wifi_config"> <child name="config_rom" /> </service>
			<service name="ROM" label="wifi.lib.so"> <parent label="pc_wifi.lib.so"/> </service>
			<service name="ROM" label="wifi_firmware.tar"> <parent label="pc_wifi_firmware.tar"/> </service>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="Uplink"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

} elseif { $use_usb_driver } {

	append config {

	<start name="usb" caps="1500" managing_system="yes" priority="-1">
		<binary name="init"/>
		<resource name="RAM" quantum="32M"/>
		<provides> <service name="Usb"/> </provides>
		<route>
			<service name="ROM" label="config">
				<parent label="drivers.config"/> </service>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="Timer">  <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="nic" caps="200" priority="-1">
		<binary name="usb_net"/>
		<resource name="RAM" quantum="20M"/>
		<config mac="02:00:00:00:01:01" />
		<route>
			<service name="Uplink"><child name="nic_router"/></service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

} else {

	append config {

	<start name="nic" caps="1200" managing_system="yes" priority="-1">
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

	<start name="nic_router" caps="120" priority="-1">
		<resource name="RAM" quantum="5M"/>
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="yes"> }

if {$use_nic_bridge} { append config {

			<policy label_prefix="nic_bridge" domain="server"/> }

} else { append config {

			<policy label_prefix="netserver_genode" domain="server"/> }
}
append config {

			<policy label_prefix="nic" domain="uplink"/>

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

	<start name="nic_bridge" priority="-1">
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

	<start name="netserver_genode" caps="320" priority="-2">
	<binary name="netserver"/>
		<resource name="RAM" quantum="14M"/>
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

#
# Define USB host controller config
#
if { $use_usb_driver } {
	set fd [open [run_dir]/genode/usb_host.config w]
	append usb_config {<config bios_handoff="yes">}
	append_if [have_board rpi] usb_config {
		<policy label_prefix="nic">
			<device vendor_id="0x0424" product_id="0xec00"/>
		</policy> }
	append_if [have_spec x86] usb_config {
		<policy label_prefix="nic">
			<device vendor_id="0x0b95" product_id="0x1790"/>
		</policy> }
	append usb_config {</config>}
	puts $fd $usb_config
	close $fd
}

build_boot_image [build_artifacts]

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

	spawn netperf-$version -4 -H $ip_addr -P 1 -v 2 -f m -t $netperf_test -c -C -- $force_ports \
	-k THROUGHPUT,THROUGHPUT_UNITS,LOCAL_TRANSPORT_RETRANS,MEAN_LATENCY,REMOTE_SEND_CALLS,REMOTE_RECV_CALLS,ELAPSED_TIME
	set netperf_id $spawn_id

	set spawn_id_list [list $netperf_id $serial_id]

	# reset output, so that we get on the second run not the result of the first
	set output ""
	run_genode_until "ELAPSED_TIME=.*\n" 120 $spawn_id_list

	set units "Mbit/s"

	# get throughput from netperf output
	regexp {THROUGHPUT=([\d\.]+)} $output dummy throughput

	# get elapsed time from netperf output
	regexp {ELAPSED_TIME=([\d\.]+)} $output dummy elapsed_time

	# get send calls from netperf output
	regexp {REMOTE_SEND_CALLS=([\d]+)} $output dummy send_calls

	# get recv calls from netperf output
	regexp {REMOTE_RECV_CALLS=([\d]+)} $output dummy recv_calls

	set sends_second [expr $send_calls / $elapsed_time]
	set recvs_second [expr $recv_calls / $elapsed_time]

	puts ""
	puts "[format %8.0f $sends_second] send()/s"
	puts "[format %8.0f $recvs_second] recv()/s"

	# format output parseable for post proccessing scripts
	puts -nonewline "! PERF: $netperf_test"
	if {$use_nic_bridge} { puts -nonewline "_bridge" }
	if {$use_usb_driver} { puts -nonewline "_xhci"   }
	puts "              $throughput $units ok"
}
