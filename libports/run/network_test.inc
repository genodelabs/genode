#
# \brief  Test ping
# \author Josef Soentgen
# \date   2013-01-06
#

#
# Client parameters
#
set packet_payload_size 24
set packet_count        1000000

#
# Build
#

set build_components {
	core init
	drivers/pci drivers/timer drivers/nic
	test/lwip/pingpong/server
}

lappend_if $use_nic_bridge build_components server/nic_bridge

lappend_if [expr [have_spec omap4] || [have_spec exynos5]] build_components drivers/usb

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
append_if [expr $use_nic_bridge && ([have_spec omap4] || [have_spec exynos5])] config {			
				<child name="usb_drv"/> }
append_if [expr $use_nic_bridge && (![have_spec omap4] && ![have_spec exynos5])] config {			
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
		</config>
		<route> }
append_if $use_nic_bridge config {
			<service name="Nic"> <child name="nic_bridge"/> </service> }
append config {
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start> }

append_if [expr [have_spec omap4] || [have_spec exynos5]] config {
	<start name="usb_drv" priority="-1">
		<resource name="RAM" quantum="12M"/>
		<provides>
			<service name="Nic"/>
		</provides>
		<config>
			<nic mac="2e:60:90:0c:4e:02" />
			<!--<nic mac="aa:bb:cc:dd:ee:00" />-->
		</config>
	</start>}

append_if [expr ![have_spec omap4] && ![have_spec exynos5]] config {
	<start name="nic_drv">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="Nic"/></provides>
	</start>}

append_if [have_spec pci] config {
	<start name="pci_drv">
		<resource name="RAM" quantum="2M"/>
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
	ld.lib.so libc.lib.so lwip.lib.so libc_log.lib.so
}

lappend_if $use_nic_bridge boot_modules nic_bridge

lappend boot_modules $test_server_name

# platform-specific modules
lappend_if [have_spec pci]           boot_modules pci_drv
lappend_if [expr [have_spec omap4] || [have_spec exynos5]] boot_modules usb_drv
lappend_if [expr ![have_spec omap4] && ![have_spec exynos5]] boot_modules nic_drv

build_boot_image $boot_modules

# establish serial connection with the server system
spawn picocom -b 115200 /dev/ttyUSB0
set server_spawn_id $spawn_id
set timeout -1
expect -i $server_spawn_id "Terminal ready"

# reset the server system (if Fiasco.OC is already running)
reset $server_spawn_id

# get IP address from server
expect {
	-i $server_spawn_id
	-re "got IP address (.*)\033.*\n" { set ip_addr $expect_out(1,string) }
}
puts "got server IP address: $ip_addr"

# wait until the server is ready
expect -i $server_spawn_id "wait...*\n"

# build the client
set pingpong_dir $genode_dir/libports/src/test/lwip/pingpong
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
expect -i $server_spawn_id "client \[1|3\] connected..."

# start counting the execution time
set time_start [clock milliseconds]

# wait until the server received all packets
expect {
	-i $server_spawn_id
	-re "received .*\n" { }
}

# stop counting the execution time
set time_end [clock milliseconds]

set milliseconds [expr $time_end - $time_start]
set payload_total [expr $packet_count * $packet_payload_size]
set payload_bytes_per_second [expr $payload_total * 1000 / $milliseconds ]

set test_result "$milliseconds ms ($payload_bytes_per_second payload bytes per second)"

puts "\nTest succeeded in $test_result."

# disconnect from server system
send -i $server_spawn_id "\x01\x18" ;# Ctrl-A Ctrl-X
expect {
	-i $server_spawn_id
	"Thanks for using picocom"
}

exec rm -f bin/network_test_client

# vi: set ft=tcl :
