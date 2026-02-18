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

assert {![have_include power_on/qemu]}
assert {![have_board imx53_qsb_tz]}
assert {![have_board rpi]}

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
				if {[have_spec sel4]}      { return 21; }
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


# Wifi driver

proc wifi_config { } {
	global wifi_ssid
	global wifi_psk
	return "ssid: $wifi_ssid | protection: WPA2 | passphrase: $wifi_psk | auto_connect: yes"
}

proc wifi_start_nodes { } { return [join [list {

+ start report_rom | caps: 100 | ram: 2M | priority: -1
  + provides
    + service ROM
    + service Report
  + config
    + policy | label: pci_decode -> system | report: acpi -> acpi
    + policy | label: platform -> devices | report: pci_decode -> devices

+ start acpi | caps: 250 | ram: 4M | priority: -1

+ start pci_decode | caps: 350 | ram: 2M | priority: -1
  + route
    + service ROM | label: system | + child report_rom
    + any-service
      + parent
      + any-child

+ start platform | caps: 100 | ram: 2M | managing_system: yes | priority: -1
  + provides | + service Platform
  + config | + policy | label_prefix: nic | info: yes | + pci | class: WIFI
  + route
    + service ROM | label: devices | + child report_rom
    + any-service
      + parent
      + any-child

+ start config_fs | ram: 4M | priority: -1
  + binary vfs
  + provides | + service File_system
  + config
    + policy | label_prefix: config_rom | root: /
    + vfs
      + ram
      + import
        + inline wifi_config
          + wifi_config
            + network | } [wifi_config] {

+ start config_rom | ram: 4M | priority: -1
  + binary fs_rom
  + provides | + service ROM
  + route
    + service File_system | + child config_fs
    + any-service
      + parent
      + any-child

+ start nic | caps: 470 | ram: 32M | priority: -1
  + binary wifi
  + config
  | + libc | stdout: /dev/null | stderr: /dev/log | rtc: /dev/rtc
  | + vfs
  |   + dir dev
  |     + log
  |     + null
  |     + jitterentropy random
  |     + jitterentropy urandom
  |     + inline rtc | : 2018-01-01 00:01
  |   + dir firmware | + tar wifi_firmware.tar
  + route
    + service Rtc                            | + any-child
    + service File_system                    | + child config_fs
    + service ROM | label: wifi_config       | + child config_rom
    + service ROM | label: wifi.lib.so       | + parent | label: pc_wifi.lib.so
    + service ROM | label: wifi_firmware.tar | + parent | label: pc_wifi_firmware.tar
    + service Report                         | + child report_rom
    + service Uplink                         | + child nic_router
    + any-service
      + parent
      + any-child
}] ""] }

if {!$use_wifi_driver} { proc wifi_start_nodes { } { } }


# USB-network driver

proc usb_start_nodes { } { return {

+ start usb | caps: 1500 | ram: 32M | managing_system: yes | priority: -1
  + binary init
  + provides | + service Usb
  + route
    + service ROM | label: config | + parent | label: drivers.config
    + service Report              | + child report_rom
    + service Timer               | + child timer
    + any-service                 | + parent

+ start nic | caps: 200 | ram: 20M | priority: -1
  + binary usb_net
  + config | mac: 02:00:00:00:01:01
  + route
    + service Uplink | + child nic_router
    + any-service
      + parent
      + any-child
} }

if {!$use_usb_driver} { proc usb_start_nodes { } { } }


# Ethernet driver

proc nic_start_nodes { } { return {
+ start nic | caps: 1200 | ram: 32M | managing_system: yes | priority: -1
  + binary init
  + route
    + service ROM | label: config | + parent | label: drivers.config
    + service Timer               | + child timer
    + service Uplink              | + child nic_router
    + any-service                 | + parent
} }

if {$use_wifi_driver || $use_usb_driver} { proc nic_start_nodes { } { } }


# NIC bridge and NIC router config

proc nic_bridge_config_attr { } {
	global bridge_mac
	return "mac: $bridge_mac"
}

proc nic_bridge_start_node { } { return [join [list {
+ start bridge | ram: 5M | priority: -1
  + binary nic_bridge
  + provides | + service Nic
  + config | } [nic_bridge_config_attr] {
    + default-policy
  + route
    + service Nic | + child nic_router
    + any-service
      + parent
      + any-child
}] ""] }

if {!$use_nic_bridge} { proc nic_bridge_start_node { } { } }

proc nic_router_uplink_domain_attr { } {
	global lx_ip_addr
	if {[have_spec linux]} {
		return "| interface: $lx_ip_addr/24 | gateway: 10.0.2.1"
	}
}

proc netserver_nic_target { } {
	global use_nic_bridge
	if {$use_nic_bridge} { return "bridge" } else { return "nic_router" }
}

install_config {
config | verbose: yes | prio_levels: 4
+ parent-provides
  + service ROM
  + service IRQ
  + service IO_MEM
  + service IO_PORT
  + service PD
  + service RM
  + service CPU
  + service LOG

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 200 | ram: 2M

+ start timer
  + provides | + service Timer

} [wifi_start_nodes] [usb_start_nodes] [nic_start_nodes] [nic_bridge_start_node] {

+ start nic_router | caps: 120 | ram: 5M | priority: -1
  + provides
    + service Nic
    + service Uplink
  + config | verbose_domain_state: yes
    + policy | label_prefix: bridge           | domain: server
    + policy | label_prefix: netserver_genode | domain: server
    + policy | label_prefix: nic              | domain: uplink
    + domain uplink | } [nic_router_uplink_domain_attr] {
      + nat | domain: server | tcp-ports: 100 | udp-ports: 100
      + tcp-forward | port: } [server_data_port] { | domain: server | to: 10.0.3.2
      + tcp-forward | port: } [server_ctrl_port] { | domain: server | to: 10.0.3.2
      + udp-forward | port: } [server_data_port] { | domain: server | to: 10.0.3.2
    + domain server | interface: 10.0.3.1/24 | verbose_packets: no
      + dhcp-server | ip_first: 10.0.3.2 | ip_last: 10.0.3.2 | ip_lease_time_sec: 600

+ start netserver_genode | caps: 320 | ram: 14M | priority: -2
  + binary netserver
  + config
  | + arg netserver
  | + arg -D
  | + arg -4
  | + arg -f
  | + libc | stdout: /dev/log | stderr: /dev/log | rtc: /dev/rtc | socket: /socket
  | + vfs
  |   + dir dev
  |     + log
  |     + inline rtc | : 2018-01-01 00:01
  |   + dir socket | + } [socket_fs_plugin] { | dhcp: yes
  + route
    + service Nic | + child } [netserver_nic_target] {
    + any-service
      + parent
      + any-child
-
}


#
# Define USB host controller config
#
if { $use_usb_driver } {
	lappend usb_config "config | bios_handoff: no"
	lappend_if [have_board rpi] usb_config \
		"+ policy | label_prefix: nic" \
		"  + device | vendor_id: 0x0424 | product_id: 0xec00"
	lappend_if [have_spec x86] usb_config \
		"+ policy | label_prefix: nic" \
		"  + device | vendor_id: 0x0b95 | product_id: 0x1790"
	lappend usb_config "-"

	set fd [open [run_dir]/genode/usb_host.config w]
	puts $fd [join $usb_config "\n"]
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
