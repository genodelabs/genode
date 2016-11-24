#
# \brief  Test using netperf with l4linux
# \author Sebastian Sumpf
# \date   2013-07-08
#

assert_spec foc
assert_spec 32bit

assert_spec foc_arndale

set ip [check_installed ip]
set wget [check_installed wget]
set md5 [check_installed md5sum]


if {$use_nic_bridge == 1} {
set route_nic "nic_bridge"
} else {
set route_nic "usb_drv"
}

set version "2.4.4"

# sanity check that the right version is used
set wrong_version [catch {
	spawn netperf-$version -V
	expect {
		"Netperf version $version"  { }
		eof { return }
		timeout { return }
	}
	spawn netserver-$version -V
	expect {
		"Netperf version $version"  { }
		eof { return }
		timeout { return }
	}
}]

if {$wrong_version} {
	puts -nonewline "\nPlease compile a netperf client of version $version "
	puts            "for your host system."
	puts -nonewline "The sources are available via "
	puts "ftp://ftp.netperf.org/netperf/archive/netperf-2.4.4.tar.bz2"
	puts "Please name the binary netperf-$version and netserver-$version\n"
	exit 1;
}

#
# Build
#
set build_components {
	core
	init
	drivers/usb
	drivers/timer
	drivers/uart
	server/nic_bridge
	l4linux
}

source ${genode_dir}/repos/base/run/platform_drv.inc
append_platform_drv_build_components

build $build_components

create_boot_directory

#
# Config
#
set config  {
<config>
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
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
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
}

append_platform_drv_config

append config {
	<start name="kdb_uart_drv">
		<resource name="RAM" quantum="1M"/>
		<provides>
			<service name="Terminal"/>
			<service name="Uart"/>
		</provides>
		<config><policy label_prefix="l4linux" uart="0"/></config>
	</start>
	<start name="usb_drv">
		<binary name="usb_drv"/>
		<resource name="RAM" quantum="12M"/>
		<provides>
			<service name="Nic"/>
		</provides>
		}
append config "
		<config uhci=\"$use_usb_11\" ehci=\"$use_usb_20\" xhci=\"$use_usb_30\">"
append config {
			<nic mac="02:00:00:00:01:01" />
		</config>
		<route>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
}

append config {
	<start name="l4linux">
		<resource name="RAM" quantum="512M"/>
		<config args="mem=400M console=ttyS0 l4x_rd=initrd.gz l4x_cpus=1 l4x_cpus_map=0"/>
		<route>
	}
	append config "\t\t<service name=\"Nic\"><child name=\"$route_nic\" /> </service>"
	append config {
			<any-service> <any-child /><parent/> </any-service>
		</route>
	</start>
	}

	append_if $use_nic_bridge config {
	<start name="nic_bridge">
		<resource name="RAM" quantum="6M"/>
		<provides><service name="Nic"/></provides>
		<config/>
		<route>
			<service name="Nic"> <child name="usb_drv"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
	}


append config {
</config>
}

install_config $config

#
# Boot modules
#
set boot_modules {
	core
	init
	timer
	l4linux
	initrd.gz
	usb_drv
	nic_bridge
	kdb_uart_drv
}

set uri "http://genode.org/files/l4linux/initrd-arm-netperf.gz"
if {[have_spec x86]} {
	set uri "http://genode.org/files/l4linux/busybox-initrd-x86-20140506.gz"
} elseif {[have_spec arm]} {
	set uri "http://genode.org/files/l4linux/initrd-arm-mdev.gz"
}

if {![file exists bin/initrd.gz]} {
	puts "Download initramfs ..."
	exec >& /dev/null $wget -c -O bin/initrd.gz $uri
	exec >& /dev/null $wget -O bin/initrd.gz.md5 $uri.md5
	cd bin
	exec $md5 -c initrd.gz.md5
	cd ..
}

append_platform_drv_boot_modules

build_boot_image $boot_modules


#
# Execute test case
#
set ip_match "(\[0-9]{1,3})\\.(\[0-9]{1,3})\\.(\[0-9]{1,3})\\.(\[0-9]{1,3})"

#
# Local IP
#
set ip_string [ exec $ip addr show dev eth0 ]
regexp $ip_match $ip_string ip_local


#
# Remote IP
#
run_genode_until "netif_info: register.*\n" 60
set serial_id [output_spawn_id]
sleep 8
send -i $serial_id "ip addr show dev eth0\n"
run_genode_until "inet.*\n" 3 $serial_id
regexp $ip_match $output ip_remote

puts "Using local IP: $ip_local remote IP: $ip_remote"


#
# TX (from board to local)
#
puts "\nTransmission test:"

exec netserver-$version -p 8888 &
send -i $serial_id "netperf -p 8888 -H $ip_local -c -C -t TCP_STREAM && uname\n"
run_genode_until "Linux.*" 30 $serial_id

puts "\n\n"

# get throughput_tx from netperf output
set throughput_tx [regexp -all -inline {([0-9]+[ ]+){3}[0-9]+\.[0-9]+[ ]+[0-9]+\.[0-9]+} $output]
set throughput_tx [regexp -all -inline {[0-9]+\.[0-9]+} $throughput_tx]

#
# RX
#
puts "Receive test:"

set results [exec netperf-$version -p 8888 -H $ip_remote -t TCP_STREAM]
puts $results

# get throughput from netperf output
set throughput_rx [regexp -all -inline {([0-9]+[ ]+){3}[0-9]+\.[0-9]+[ ]+[0-9]+\.[0-9]+} $results]
set throughput_rx [regexp -all -inline {[0-9]+\.[0-9]+} $throughput_rx]

puts "\n"

#
# format output parseable for post proccessing scripts
#
puts -nonewline "! PERF: TCP_TX"
if {$use_nic_bridge} { puts -nonewline "_bridge" }
if {![string compare $use_usb_11 "yes"]} { puts -nonewline "_uhci" }
if {![string compare $use_usb_20 "yes"]} { puts -nonewline "_ohci" }
if {![string compare $use_usb_30 "yes"]} { puts -nonewline "_xhci" }
puts "              [lindex $throughput_tx 1] MBit/s ok"

puts -nonewline "! PERF: TCP_RX"
if {$use_nic_bridge} { puts -nonewline "_bridge" }
if {![string compare $use_usb_11 "yes"]} { puts -nonewline "_uhci" }
if {![string compare $use_usb_20 "yes"]} { puts -nonewline "_ohci" }
if {![string compare $use_usb_30 "yes"]} { puts -nonewline "_xhci" }
puts "              [lindex $throughput_rx 1] MBit/s ok"

exec rm bin/initrd.gz bin/initrd.gz.md5

close -i $serial_id
exec killall netserver-$version

# vi: set ft=tcl :
