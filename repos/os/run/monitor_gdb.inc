proc platform_supported { } {
	if {[have_spec x86_64] && [have_board pc]} {
		if {[have_spec nova] || [have_spec hw]} {
			return 1 }
	} elseif {[have_spec arm_v8a] && [have_board rpi3] &&
	          [have_include power_on/qemu]} {
		if {[have_spec hw]} {
			return 1 }
	}
	return 0
}

if {![platform_supported]} {
	puts "Run script is not supported on this platform"
	exit 0
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/init \
                  [depot_user]/src/sandbox \
                  [depot_user]/src/monitor

set build_components { lib/ld test/monitor_gdb test/log }

if {[have_include power_on/qemu]} {
	append build_components { driver/uart }
} else {
	import_from_depot [depot_user]/pkg/[drivers_nic_pkg] \
                      [depot_user]/src/nic_router \
                      [depot_user]/src/vfs \
                      [depot_user]/src/vfs_lwip \
                      [depot_user]/src/vfs_pipe

	append build_components { server/tcp_terminal lib/libc lib/libm }
}

build $build_components

append config {
<config>
	<parent-provides>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="CPU"/>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="RM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
}

if { [have_include power_on/qemu] } {

	append_if [have_board pc] config {
		<start name="terminal">
			<binary name="pc_uart"/>
			<resource name="RAM" quantum="2M"/>
			<provides>
				<service name="Terminal"/>
				<service name="Uart"/>
			</provides>
			<config>
				<policy label_prefix="monitor" uart="1"/>
			</config>
		</start>
	}

	append_if [have_board rpi3] config {
		<start name="terminal">
			<binary name="rpi3_uart"/>
			<resource name="RAM" quantum="2M"/>
			<provides>
				<service name="Terminal"/>
				<service name="Uart"/>
			</provides>
			<config>
				<policy label_prefix="monitor" uart="0"/>
			</config>
		</start>
	}

} else {

	append config {
		<start name="drivers" caps="1000" managing_system="yes">
			<resource name="RAM" quantum="32M"/>
			<binary name="init"/>
			<route>
				<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
				<service name="Timer"> <child name="timer"/> </service>
				<service name="Uplink"> <child name="nic_router"/> </service>
				<any-service> <parent/> </any-service>
			</route>
		</start>
		<start name="nic_router" caps="200">
			<resource name="RAM" quantum="10M"/>
			<provides>
				<service name="Nic"/>
				<service name="Uplink"/>
			</provides>
			<config verbose_domain_state="yes">

				<policy label_prefix="terminal" domain="downlink"/>
				<policy label_prefix="drivers"  domain="uplink"/>

				<domain name="uplink">

					<nat domain="downlink"
						tcp-ports="1"
						udp-ports="1"
						icmp-ids="1"/>

					<tcp-forward port="5555" domain="downlink" to="10.0.3.2"/>

				</domain>

				<domain name="downlink" interface="10.0.3.1/24">

					<dhcp-server ip_first="10.0.3.2" ip_last="10.0.3.2"/>

					<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>

				</domain>

			</config>
		</start>
		<start name="terminal" caps="200">
			<binary name="tcp_terminal"/>
			<resource name="RAM" quantum="8M"/>
			<provides> <service name="Terminal"/> </provides>
			<config>
				<policy label_prefix="monitor" port="5555"/>
				<vfs>
					<dir name="dev"> <log/> </dir>
					<dir name="socket"> <lwip dhcp="yes"/> </dir>
					<dir name="pipe"> <pipe/> </dir>
				</vfs>
				<libc stdout="/dev/log" socket="/socket" pipe="/pipe"/>
			</config>
			<route>
				<service name="Nic"> <child name="nic_router"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
		</start>
	}
}

append config {

	<start name="monitor" caps="1000">
		<resource name="RAM" quantum="100M"/>
		<config>
			<parent-provides>
				<service name="LOG"/>
				<service name="PD"/>
				<service name="CPU"/>
				<service name="ROM"/>
			</parent-provides>
			<default caps="100"/>

			<monitor>
				<policy label="test-monitor_gdb" wait="yes" wx="yes"/>
			</monitor>

			<start name="test-monitor_gdb" caps="300">
				<resource name="RAM" quantum="10M"/>
				<config>
					<vfs> <dir name="dev"> <log/> </dir> </vfs>
					<libc stdout="/dev/log" stderr="/dev/log"/>
				</config>
				<route>
					<service name="PD">  <local/> </service>
					<service name="CPU"> <local/> </service>
					<any-service> <parent/> </any-service>
				</route>
			</start>
			<start name="test-log">
				<resource name="RAM" quantum="2M"/>
				<route>
					<service name="PD">  <local/> </service>
					<service name="CPU"> <local/> </service>
					<any-service> <parent/> </any-service>
				</route>
			</start>
		</config>
	</start>
</config>
}

install_config $config

build_boot_image [build_artifacts]

set port 5555

if {[have_include power_on/qemu]} {

	set host "localhost"

	# qemu config
	append qemu_args " -display none "

	if {[have_board rpi3]} {
		# connect comport 0 with TCP port $port
		append qemu_args " -serial chardev:uart "
		# connect comport 1 to stdio
		append qemu_args " -serial stdio "
	} else {
		# connect comport 0 to stdio
		append qemu_args " -serial stdio "
		# connect comport 1 with TCP port $port
		append qemu_args " -serial chardev:uart "
	}

	append qemu_args " -chardev socket,id=uart,port=$port,host=$host,server,nowait,ipv4 "

	run_genode_until {.*monitor ready*} 30

} else {

	set match_string "nic_router. .uplink. dynamic IP config: interface .*\n"

	run_genode_until $match_string 30

	regexp $match_string $output host
	regexp {[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} $host host
}

set genode_id [output_spawn_id]

# GDB loads symbols from 'debug/ld.lib.so'
if { [have_spec nova] } {
	exec ln -sf ld-nova.lib.so debug/ld.lib.so
} elseif { [have_spec hw] } {
	exec ln -sf ld-hw.lib.so debug/ld.lib.so
}
