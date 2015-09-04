#
# \brief  Test ping
# \author Josef Soentgen
# \date   2013-01-06
#

#
# Client parameters
#
set packet_payload_size 10
set packet_count        1000000

if {[have_spec hw_odroid_xu]} {
	puts "Run script does not support this platform."
	exit 0
}

#
# Build
#

set build_components {
	core init
	drivers/timer drivers/nic
	test/lwip/pingpong/server
}

lappend_if $use_nic_bridge build_components server/nic_bridge

lappend_if [expr [have_spec omap4] || [have_spec arndale]] build_components drivers/platform
lappend_if [expr [have_spec omap4] || [have_spec arndale]] build_components drivers/usb
lappend_if [have_spec acpi]  build_components drivers/acpi
lappend_if [have_spec pci]   build_components drivers/pci/device_pd
lappend_if [have_spec pci]   build_components drivers/pci

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

append_if $use_nic_bridge config {
	<start name="nic_bridge">
		<resource name="RAM" quantum="2M"/>
		<provides><service name="Nic"/></provides>
		<route>
			<service name="Nic"> }
append_if [expr $use_nic_bridge && ([have_spec omap4] || [have_spec arndale])] config {
				<child name="usb_drv"/> }
append_if [expr $use_nic_bridge && (![have_spec omap4] && ![have_spec arndale])] config {
				<child name="nic_drv"/> }
append_if $use_nic_bridge config {
			</service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

append config "
	<start name=\"$test_server_name\">"
append config {
		<resource name="RAM" quantum="16M"/>
		<config>
			<argv verbose="0" listenip="0.0.0.0" />
			<libc stdout="/dev/log" stderr="/dev/log">
				<vfs> <dir name="dev"> <log/> </dir> </vfs>
			</libc>
		</config>
		<route> }
append_if $use_nic_bridge config {
			<service name="Nic"> <child name="nic_bridge"/> </service> }
append config {
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

append_if [have_spec arndale] config {
	<start name="platform_drv">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="Regulator"/></provides>
	</start>}

append_if [expr [have_spec omap4] || [have_spec arndale]] config {
	<start name="usb_drv">
		<resource name="RAM" quantum="12M"/>
		<provides>
			<service name="Nic"/>
		</provides>
		<config ehci="yes">
			<nic mac="02:00:00:00:01:01" />
			<!--<nic mac="02:00:00:00:01:01" />-->
		</config>
	</start>}

append_if [expr ![have_spec omap4] && ![have_spec arndale]] config {
	<start name="nic_drv">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="Nic"/></provides>
	</start>}

append_if [have_spec acpi] config {
	<start name="acpi">
		<resource name="RAM" quantum="6M" constrain_phys="yes"/>
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
		<resource name="RAM" quantum="3M" constrain_phys="yes"/>
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
	ld.lib.so libc.lib.so lwip.lib.so
}

lappend_if $use_nic_bridge boot_modules nic_bridge

lappend boot_modules $test_server_name

# platform-specific modules
lappend_if [have_spec pci]           boot_modules pci_drv
lappend_if [have_spec acpi]          boot_modules acpi_drv
lappend_if [expr [have_spec omap4] || [have_spec arndale]] boot_modules usb_drv
lappend_if [expr ![have_spec omap4] && ![have_spec arndale]] boot_modules nic_drv
lappend_if [have_spec nova]          boot_modules pci_device_pd
lappend_if [have_spec arndale]       boot_modules platform_drv

build_boot_image $boot_modules

append qemu_args " -net user,hostfwd=tcp::10000-:10000 "
append qemu_args " -nographic -serial mon:stdio "

# get IP address from server
set match_string "got IP address (.*)\033.*\n"
run_genode_until $match_string 30
if {[have_include power_on/qemu]} {
	set server_spawn_id $output_spawn_id
	set ip_addr "127.0.0.1"
} else {
	set server_spawn_id $spawn_id
	regexp $match_string $output all ip_addr
}

puts "got server IP address: $ip_addr"

set match_string "wait....*\n"
if {![regexp $match_string $output] } {
	# wait until the server is ready
	run_genode_until $match_string 30 $server_spawn_id
}

# build the client
set pingpong_dir $genode_dir/repos/libports/src/test/lwip/pingpong
exec g++ -o bin/network_test_client \
         $pingpong_dir/client/main.cc \
         $pingpong_dir/pingpong.cc \
         -I $pingpong_dir

# start the client
spawn bin/network_test_client \
	-serverip $ip_addr \
	-startsize $packet_payload_size \
	-endsize $packet_payload_size \
	-count $packet_count

# wait until the client is connected to the server
run_genode_until "client \[1|3\] connected..." 60 $server_spawn_id

# start counting the execution time
set time_start [clock milliseconds]

# wait until the server received all packets
run_genode_until "received .*\n" 600 $server_spawn_id

# stop counting the execution time
set time_end [clock milliseconds]

# get received packet size and count from target
set target_packet_size [regexp -inline {received [0-9]+ packets of size [0-9]+} $output]
set target_packet_size [regexp -all -inline {[0-9]+} $target_packet_size]
set target_packet_count [lindex $target_packet_size 0]
set target_packet_size  [lindex $target_packet_size 1]

# calculate troughput
set milliseconds [expr $time_end - $time_start]
set payload_total [expr $target_packet_count * $target_packet_size]
set payload_bytes_per_second [expr $payload_total * 1000 / $milliseconds ]


puts "! PERF: $target_packet_size\_data [expr $payload_bytes_per_second * 8 / 1000 / 1000] MBit/s ok"

set test_result "$milliseconds ms. payload ($target_packet_size Byte): $payload_bytes_per_second Byte/s"

puts "\nTest succeeded in $test_result."

# disconnect from server system
if {[expr [have_spec omap4] || [have_spec arndale]]} {
	send -i $server_spawn_id "\x01\x18" ;# Ctrl-A Ctrl-X
}

set qemu_args ""
exec rm -f bin/network_test_client

# vi: set ft=tcl :
