proc platform_supported { } {
	if {[have_spec x86_64] && [have_board pc]} {
		if {[have_spec nova] || [have_spec hw]} {
			return 1 }
	} elseif {[have_spec arm_v8a] && [have_board rpi3]} {
		if {[have_spec hw]} {
			return 1 }
	}
	return 0
}

if {![platform_supported]} {
	puts "Run script is not supported on this platform"
	exit 0
}

proc gdb_pkg_name { } {
	if {[have_spec arm_64]} {
		return "gdb_arm_64"
	} elseif {[have_spec x86]} {
		return "gdb_x86"
	}
}

proc gdb_prefix { } {
	if {[have_spec arm_64]} {
		return "genode-aarch64-"
	} elseif {[have_spec x86]} {
		return "genode-x86-"
	}
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/init \
                  [depot_user]/src/sandbox \
                  [depot_user]/src/monitor \
                  [depot_user]/src/vfs \
                  [depot_user]/src/fs_rom \
                  [depot_user]/src/vfs_pipe \
                  [depot_user]/src/terminal_crosslink \
                  [depot_user]/src/libc \
                  [depot_user]/src/posix \
                  [depot_user]/src/socat \
                  [depot_user]/src/expat \
                  [depot_user]/src/gmp \
                  [depot_user]/src/ncurses \
                  [depot_user]/src/stdcxx \
                  [depot_user]/src/[gdb_pkg_name]

if {[have_include power_on/qemu]} {
	append build_components { driver/uart }
} else {
	import_from_depot [depot_user]/pkg/[drivers_nic_pkg] \
                      [depot_user]/src/nic_router \
                      [depot_user]/src/vfs_lwip
}

append build_components { lib/ld test/monitor_gdb test/log }

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
		<start name="uart">
			<binary name="pc_uart"/>
			<resource name="RAM" quantum="2M"/>
			<provides>
				<service name="Terminal"/>
				<service name="Uart"/>
			</provides>
			<config>
				<policy label_prefix="socat" uart="1"/>
			</config>
		</start>
	}

	append_if [have_board rpi3] config {
		<start name="uart">
			<binary name="rpi3_uart"/>
			<resource name="RAM" quantum="2M"/>
			<provides>
				<service name="Terminal"/>
				<service name="Uart"/>
			</provides>
			<config>
				<policy label_prefix="socat" uart="0"/>
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

				<policy label_prefix="socat"   domain="downlink"/>
				<policy label_prefix="drivers" domain="uplink"/>

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
	}
}

append config {

	<start name="terminal_monitor">
		<binary name="terminal_crosslink"/>
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Terminal"/>
		</provides>
	</start>

	<start name="terminal_gdb">
		<binary name="terminal_crosslink"/>
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Terminal"/>
		</provides>
	</start>

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
		<route>
			<service name="Terminal"> <child name="terminal_monitor"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="vfs">
		<resource name="RAM" quantum="12M"/>
		<provides> <service name="File_system"/> </provides>
		<config>
			<vfs>
				<tar name="socat.tar"/>
				<tar name="gdb.tar"/>
			</vfs>
			<default-policy root="/" writeable="no"/>
		</config>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="vfs_rom">
		<resource name="RAM" quantum="60M"/>
		<binary name="fs_rom"/>
		<provides> <service name="ROM"/> </provides>
		<config/>
		<route>
			<service name="File_system"> <child name="vfs"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="socat" caps="200">
		<binary name="/bin/socat"/>
		<resource name="RAM" quantum="30M" />
		<config>
			<vfs>
				<dir name="dev">
					<log/>
					<inline name="rtc">2018-01-01 00:01</inline>
					<terminal name="gdb" label="gdb"/>}
append_if [have_include power_on/qemu] config {
					<terminal name="uart" label="uart"/>
}
append config {
				</dir>}
if {![have_include power_on/qemu]} {
	append config {
				<dir name="socket"> <lwip dhcp="yes"/> </dir>
	}
}
append config {
				<fs/>
			</vfs>
			<libc stdout="/dev/log"
			      stderr="/dev/log"
			      rtc="/dev/rtc"
			      socket="/socket"/>
			<arg value="socat"/>
			<arg value="/dev/gdb"/>}
if {[have_include power_on/qemu]} {
	append config {
			<arg value="/dev/uart"/>
	}
} else {
	append config {
			<arg value="TCP4-LISTEN:5555"/>
	}
}
append config {
		</config>
		<route>
			<service name="File_system"> <child name="vfs"/> </service>
			<service name="ROM" label_suffix=".lib.so"> <parent/> </service>
			<service name="ROM" label_last="/bin/socat"> <child name="vfs_rom"/> </service>
			<service name="Terminal" label="uart">
				<child name="uart"/>
			</service>
			<service name="Terminal" label="gdb">
				<child name="terminal_gdb"/>
			</service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="gdb" caps="300">
		<binary name="/bin/} [gdb_prefix] {gdb"/>
		<resource name="RAM" quantum="40M"/>
		<config ld_verbose="yes">
			<vfs>
				<dir name="dev">
					<log/>
					<inline name="rtc">2023-01-01 00:01</inline>
					<terminal name="tty" label="tty"/>
					<terminal name="monitor" label="monitor"/>
				</dir>
				<dir name="pipe"> <pipe/> </dir>
				<tar name="gdb_run_debug.tar"/>
				<fs/>
			</vfs>
			<libc stdin="/dev/tty" stdout="/dev/tty" stderr="/dev/tty"
			      pipe="/pipe" rtc="/dev/rtc"/>
			<arg value="gdb"/>
			<arg value="debug/ld.lib.so"/>
			<arg value="-ex"/> <arg value="set pagination off"/>
			<arg value="-ex"/> <arg value="set style enabled off"/>
			<arg value="-ex"/> <arg value="set interactive-mode off"/>
			<arg value="-ex"/> <arg value="set solib-search-path debug"/>
			<arg value="-ex"/> <arg value="set non-stop on"/>
			<arg value="-ex"/> <arg value="target extended-remote /dev/monitor"/>
			<arg value="-ex"/> <arg value="b binary_ready_hook_for_gdb"/>
			<arg value="-ex"/> <arg value="c"/>
			<arg value="-ex"/> <arg value="delete 1"/>
			<arg value="-ex"/> <arg value="thread 2"/>
			<arg value="-ex"/> <arg value="file debug/test-monitor_gdb"/>
		</config>
		<route>
			<service name="ROM" label_last="/bin/} [gdb_prefix] {gdb">
				<child name="vfs_rom"/>
			</service>
			<service name="Terminal" label="tty">
				<child name="terminal_gdb"/>
			</service>
			<service name="Terminal" label="monitor">
				<child name="terminal_monitor"/>
			</service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

</config>
}

install_config $config

# GDB loads symbols from 'debug/ld.lib.so'
if { [have_spec nova] } {
	exec ln -sf ld-nova.lib.so debug/ld.lib.so
	exec tar cf bin/gdb_run_debug.tar -h debug/ld-nova.lib.so.debug
} elseif { [have_spec hw] } {
	exec ln -sf ld-hw.lib.so debug/ld.lib.so
	exec tar cf bin/gdb_run_debug.tar -h debug/ld-hw.lib.so.debug
}

exec tar uf bin/gdb_run_debug.tar -h \
	debug/ld.lib.so \
	debug/test-monitor_gdb \
	debug/test-monitor_gdb.debug \
	debug/test-log \
	debug/test-log.debug

build_boot_image "[build_artifacts] gdb_run_debug.tar"

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
