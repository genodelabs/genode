#
# \brief  Nic performance test
# \author Alexander Boettcher
# \date   2013-03-26
#

if {[have_spec linux]} {
	puts "Run script does not support Linux."; exit 0 }

#
# Build
#

set build_components {
	core init
	drivers/pci drivers/timer drivers/nic
}

lappend_if [expr [have_spec omap4] || [have_spec exynos5]] build_components drivers/usb
lappend_if [expr [have_spec omap4] || [have_spec exynos5]] build_components drivers/usb_net_stat
lappend_if [have_spec acpi] build_components drivers/acpi
lappend_if [have_spec pci] build_components drivers/pci/device_pd
lappend_if [have_spec x86] build_components drivers/nic_stat

lappend_if $use_nic_client build_components test/nic_raw
lappend_if $use_nic_bridge build_components server/nic_bridge

#
# Choose network driver
#
if {[expr $use_nic_client && ([have_spec omap4] || [have_spec exynos5])]} {
	set network_driver "usb_drv" }
if {[expr !$use_nic_client && ([have_spec omap4] || [have_spec exynos5])]} {
	set network_driver "usb_drv_net_stat" }
if {[expr $use_nic_client && ![have_spec omap4] && ![have_spec exynos5]]} {
	set network_driver "nic_drv" }
if {[expr !$use_nic_client && ![have_spec omap4] && ![have_spec exynos5]]} {
	set network_driver "nic_drv_stat" }

build $build_components

create_boot_directory

#
# Generate config
#

set config {
<config verbose="yes">
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="CAP"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start> }

append_if [expr [have_spec omap4] || [have_spec exynos5]] config "
	<start name=\"$network_driver\">
		<resource name=\"RAM\" quantum=\"12M\"/>
		<provides>
			<service name=\"Nic\"/>
		</provides>
		<config>
			<nic mac=\"02:00:00:00:01:01\" />
		</config>
	</start>"

append_if [expr ![have_spec omap4] && ![have_spec exynos5]] config "
	<start name=\"$network_driver\">
		<resource name=\"RAM\" quantum=\"4M\"/>
		<provides><service name=\"Nic\"/></provides>
	</start>"

append_if $use_nic_bridge config {
	<start name="nic_bridge">
		<resource name="RAM" quantum="2M"/>
		<provides><service name="Nic"/></provides>
		<route>
			<service name="Nic"> }
append_if $use_nic_bridge config "
			<child name=\"$network_driver\"/>"
append_if $use_nic_bridge config {
			</service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }
	
append_if $use_nic_client config {
	<start name="test-nic_raw">
		<resource name="RAM" quantum="4M"/>
		<route> }
append_if [expr $use_nic_client && $use_nic_bridge] config {
			<service name="Nic"> <child name="nic_bridge"/> </service> }
append_if $use_nic_client config {
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

append_if [have_spec acpi] config {
	<start name="acpi">
		<resource name="RAM" quantum="6M"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if [expr ![have_spec acpi] && [have_spec pci]] config {
	<start name="pci_drv">
		<resource name="RAM" quantum="3M"/>
		<provides> <service name="PCI"/> </provides>
	</start> }

append config {
</config>
}

install_config $config

#
# Boot modules
#

# generic modules
set boot_modules {
	core init timer
}

lappend_if $use_nic_client boot_modules test-nic_raw
lappend_if $use_nic_bridge boot_modules nic_bridge

# platform-specific modules
lappend_if [have_spec acpi]          boot_modules acpi_drv
lappend_if [have_spec pci]           boot_modules pci_drv
lappend_if [expr [have_spec omap4] || [have_spec exynos5]] boot_modules usb_drv
lappend_if [expr [have_spec omap4] || [have_spec exynos5]] boot_modules usb_drv_net_stat
lappend_if [expr ![have_spec omap4] && ![have_spec exynos5]] boot_modules nic_drv
lappend_if [expr ![have_spec omap4] && ![have_spec exynos5]] boot_modules nic_drv_stat
lappend_if [have_spec nova]          boot_modules pci_device_pd

build_boot_image $boot_modules

# build the test client
set test_client_dir $genode_dir/os/src/lib/net
if {![file exists bin/nic_drv_test_client]} {
	exec g++ -Wall -o bin/nic_drv_test_client $test_client_dir/stat-client.cc
}
# set appropriate rights to create raw sockets
set cap_rights [exec getcap bin/nic_drv_test_client]
if {![regexp cap_net_raw $cap_rights]} {
	set sudo_command "sudo setcap cap_net_raw=ep bin/nic_drv_test_client"
	puts "For sending raw packets the Linux capability for cap_net_raw is required."
	puts "Trying to execute: '$sudo_command'"
	eval "exec $sudo_command"
}

#
# Execute test case
#

if {[expr [have_spec omap4] || [have_spec exynos5]]} {
	run_genode_until {announces service "Nic"} 30
	set serial_id $spawn_id 

	set match_string "Using configured mac: "

	sleep 10
} else {
	set match_string {--- get MAC address }
	run_genode_until "$match_string.*\n" 30
	set serial_id $spawn_id

	sleep 2

}

# remember output string, will be overwritten by next run_genode_until
set mac_string $output

if {$use_nic_client} {
	# wait until client become ready
	if {![regexp {ready to receive packets} $output]} {
		run_genode_until {ready to receive packets} 10 $serial_id
	}
}

if {$use_nic_bridge} {
	set match_string "mac: "
	if {![regexp "$match_string.*\n" $output]} {
		run_genode_until "$match_string.*\n" 10 $serial_id
	}
}

grep_output $match_string
unify_output {[\r\n\t]+} ""
unify_output {[\033]\[0m} ""

set pos [string first $match_string $mac_string]
set mac_addr [string replace $mac_string 0 [expr $pos + [string length $match_string] - 1]]

#
# Start test with different packet size 
#
set test_result ""
set packet_size_list [list 64 128 256 512 1024 1500]
foreach packet_size $packet_size_list {
	set packet_count [expr 200 * 1024 * 1024 / $packet_size]

	set time_start [clock milliseconds] 
	spawn bin/nic_drv_test_client eth0 $mac_addr $packet_size $packet_count
	set test_app_id $spawn_id 
	interact $test_app_id

	run_genode_until "packets dropped.*\n" 15 $serial_id
	run_genode_until "packets dropped.*\n" 70 $serial_id
	set time_end [clock milliseconds] 

	set target_data [regexp -inline {[0-9]+ kiBytes} $output]
	set target_data [regexp -inline {[0-9]+} $target_data]

	set match_string {[0-9]+ kBit/s \(raw [0-9]+ kBit/s\), runtime [0-9]+ ms}
	set target_perf [regexp -inline $match_string $output]
	set target_perf [regexp -all -inline {[0-9]+} $target_perf]
	set target_runtime [lindex $target_perf 2]
	set target_perf [lindex $target_perf 0]

	set host_data [expr $packet_count * $packet_size / 1024 ]
#	set host_perf [expr $host_data * 1024 * 8 / ($time_end - $time_start)]
	set host_perf [expr $target_data * 1024 * 8 / ($time_end - $time_start)]

	puts "host  : data send     - $host_data kiBytes"
	puts "target: data received - $target_data kiBytes"
	puts "host  : runtime [expr $time_end - $time_start] ms $host_perf kBit/s"
	puts "target: runtime $target_runtime ms $target_perf kBit/s"
	puts "! PERF: rx_$packet_size [expr $host_perf / 1000] MBit/s ok"
	set test_result "$test_result rx_$packet_size:$host_perf"
}
close -i $serial_id

puts ""
puts "Test succeeded"

# vi: set ft=tcl :
