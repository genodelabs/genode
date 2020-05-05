#
# \brief  Test for using netperf
# \author Alexander Boettcher
# \author Martin Stein
# \date   2013-04-18
#

if {[expr $use_nic_bridge && $use_nic_router]} {
	puts "Cannot test with both NIC bridge and NIC router.";
	exit 0
}

if {[get_cmd_switch --autopilot] && [have_include "power_on/qemu"]} {
	puts "\nRunning netperf in autopilot on Qemu is not recommended.\n"
	exit
}

if {[have_include "power_on/qemu"] &&
    [expr $use_nic_bridge || $use_nic_router || $use_usb_driver || $use_wifi_driver]} {
	puts "Cannot test this configuration on qemu.";
	exit 0
}

if {[expr [have_spec imx53] && [have_spec trustzone]]} {
	puts "Run script is not supported on this platform."
	exit 0
}

set bridge_mac "02:02:02:02:02:00"

# autopilot: configure disjoint mac-address ranges for x86_32, x86_64, and others
if {[get_cmd_switch --autopilot]} {
	if {[have_spec x86_32]} { set bridge_mac "02:02:02:02:32:00" }
	if {[have_spec x86_64]} { set bridge_mac "02:02:02:02:64:00" }
}

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
"

append_if [expr !$use_lxip] packages " [depot_user]/src/vfs_lwip          "
append_if $use_lxip         packages " [depot_user]/src/vfs_lxip          "
append_if $use_nic_bridge   packages " [depot_user]/src/nic_bridge        "
append_if $use_nic_router   packages " [depot_user]/src/nic_router        "
append_if $use_usb_driver   packages " [depot_user]/src/platform_drv      "
append_if $use_usb_driver   packages " [depot_user]/src/usb_drv           "
append_if $use_wifi_driver  packages " [depot_user]/src/fs_rom            "
append_if $use_wifi_driver  packages " [depot_user]/src/vfs_import        "
append_if $use_wifi_driver  packages " [depot_user]/src/report_rom        "
append_if $use_wifi_driver  packages " [depot_user]/src/wifi_drv          "
append_if $use_wifi_driver  packages " [depot_user]/src/libcrypto         "
append_if $use_wifi_driver  packages " [depot_user]/src/libssl            "
append_if $use_wifi_driver  packages " [depot_user]/src/vfs_jitterentropy "
append_if $use_wifi_driver  packages " [depot_user]/raw/wifi_firmware     "

import_from_depot $packages

build { app/netperf }

#
# Generate config
#

set lx_ip_addr "10.0.2.55"

source ${genode_dir}/repos/base/run/platform_drv.inc
proc platform_drv_policy {} {
	global use_wifi_driver
	if { $use_wifi_driver } {
		return {<policy label_prefix="nic_drv"> <pci class="WIFI"/> </policy>}}
	return {<policy label_prefix="nic_drv"> <pci class="USB"/> </policy>}
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
		<resource name="RAM" quantum="24M"/>
		<provides> <service name="Nic"/> </provides>
		<config>
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
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

} elseif { $use_usb_driver } {
	append_platform_drv_config
	append config {
	<start name="nic_drv" caps="150">
		<binary name="usb_drv"/>
		<resource name="RAM" quantum="18M"/>
		<provides><service name="Nic"/></provides>
		<config uhci="no" ehci="no" xhci="yes">
			<nic mac="02:00:00:00:01:01" />
		</config>
	</start> }
} else {
	append config {
	<start name="nic_drv" caps="1000">
		<resource name="RAM" quantum="32M" constrain_phys="yes"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<provides> <service name="Nic"/> </provides>
	</start> }
}

append_if $use_nic_router config {
	<start name="nic_router" caps="120">
		<resource name="RAM" quantum="5M"/>
		<provides><service name="Nic"/></provides>
		<config verbose_domain_state="yes">

			<policy label_prefix="netserver_genode" domain="server"/>
			<uplink                                 domain="uplink"/>

			<domain name="uplink"}
append_if [expr $use_nic_router && [have_spec linux]] config "
				interface=\"$lx_ip_addr/24\" gateway=\"10.0.2.1\""
append_if $use_nic_router config {
			>
				<nat domain="server" tcp-ports="100" />
				<tcp-forward port="12864" domain="server" to="10.0.3.2" />
				<tcp-forward port="12865" domain="server" to="10.0.3.2" />
			</domain>

			<domain name="server" interface="10.0.3.1/24">
				<dhcp-server ip_first="10.0.3.2"
				             ip_last="10.0.3.2"
				             ip_lease_time_sec="600"/>
			</domain>

		</config>
		<route>
			<service name="Nic"> <child name="nic_drv"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
}

append_if $use_nic_bridge config {
	<start name="nic_bridge">
		<resource name="RAM" quantum="5M"/>
		<provides><service name="Nic"/></provides>
		<config mac="} $bridge_mac {">}
append_if [expr $use_nic_bridge && [have_spec linux]] config "
			<policy label_prefix=\"netserver_genode\" ip_addr=\"$lx_ip_addr\"/>"
append_if $use_nic_bridge config {
			<default-policy/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_drv"/> </service>
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
					<} [socket_fs_plugin] { }
if {[expr [have_spec linux] && !$use_nic_router]} {
	append config " ip_addr=\"$lx_ip_addr\" netmask=\"255.255.255.0\" gateway=\"10.0.2.1\""
} else {
	append config " dhcp=\"yes\""
}
append config {/>
				</dir>
			</vfs>
		</config>}
append_if $use_nic_bridge config {
		<route>
			<service name="Nic"> <child name="nic_bridge"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>}
append_if $use_nic_router config {
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>}
append config {
	</start>
</config>
}

install_config $config

build_boot_image { netserver }

#
# Execute test case
#

# qemu config
append qemu_args "  -nographic "

proc qemu_nic_model {} {
	if [have_spec x86]     { return e1000 }
	if [have_spec lan9118] { return lan9118 }
	return nic_model_missing
}

append qemu_args " -netdev user,id=net0,hostfwd=tcp::12865-:12865,hostfwd=tcp::49153-:49153 "
append qemu_args " -net nic,model=[qemu_nic_model],netdev=net0 "

set ip_match_string ""
set force_ports ""
if $use_lxip {
	set ip_match_string "ipaddr=(\[0-9\]+\.\[0-9\]+\.\[0-9\]+\.\[0-9\]+).*\n"
} else {
	set ip_match_string "address=(\[0-9\]+\.\[0-9\]+\.\[0-9\]+\.\[0-9\]+).*\n"
}
if $use_nic_router {
	set ip_match_string "nic_router\\\] \\\[uplink\\\] dynamic IP config: interface (\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}).*\n"
	set force_ports "-P 12864,12864"
}

if {[have_spec linux]} {
	run_genode_until {.*family AF_INET.*\n} 60
} else {
	run_genode_until $ip_match_string 60
}

set serial_id [output_spawn_id]

if {[have_include "power_on/qemu"]} {
	set ip_addr "localhost"
	set force_ports "-P 49153,49153"
} elseif [have_spec linux] {
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
	if {$use_nic_router} { puts -nonewline "_router" }
	if {$use_usb_driver} { puts -nonewline "_xhci"   }
	puts "              [lindex $throughput 1] MBit/s ok"
}
