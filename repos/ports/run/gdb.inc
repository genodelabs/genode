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

assert {[platform_supported]}

proc socat_uart { } { return [have_include power_on/qemu] }

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

if {[socat_uart]} {
	append build_components { driver/uart }
} else {
	import_from_depot [depot_user]/pkg/[drivers_nic_pkg] \
	                  [depot_user]/src/nic_router \
	                  [depot_user]/src/vfs_lwip
}

append build_components { lib/ld test/monitor_gdb test/log }

build $build_components

proc pc_uart_start_node { } { return {
+ start uart | ram: 2M
  + binary pc_uart
  + provides | + service Terminal
  + config | + policy | label_prefix: socat | uart: 1
} }

proc rpi3_uart_start_node { } { return {
+ start uart | ram: 2M
  + binary rpi3_uart
  + provides | + service Terminal
  + config | + policy | label_prefix: socat | uart: 0
} }

proc uart_start_node { } {
	if {[socat_uart]} {
		if {[have_board pc]}   { return [pc_uart_start_node] }
		if {[have_board rpi3]} { return [rpi3_uart_start_node] }
	}
}

proc nic_start_nodes { } { return {
+ start drivers | caps: 1000 | ram: 32M | managing_system: yes
  + binary init
  + route
    + service ROM | label: config | + parent | label: drivers.config
    + service Timer               | + child timer
    + service Uplink              | + child nic_router
    + any-service                 | + parent

+ start nic_router | caps: 200 | ram: 10M
  + provides
    + service Nic
    + service Uplink
  + config | verbose_domain_state: yes
    + policy | label_prefix: socat   | domain: downlink
    + policy | label_prefix: drivers | domain: uplink
    + domain uplink
      + nat | domain: downlink | tcp-ports: 1 | udp-ports: 1 | icmp-ids: 1
      + tcp-forward | port: 5555 | domain: downlink | to: 10.0.3.2
    + domain downlink | interface: 10.0.3.1/24
      + dhcp-server | ip_first: 10.0.3.2 | ip_last: 10.0.3.2
      + tcp | dst: 0.0.0.0/0 | + permit-any | domain: uplink
} }

if {[socat_uart]} { proc nic_start_nodes { } { } }

proc socat_uart_dev { } { if {[socat_uart]}  { return "+ terminal uart | label: uart" } }
proc socat_tcpip    { } { if {![socat_uart]} { return "+ lwip | dhcp: yes" } }

proc socat_last_arg { } {
	if {[socat_uart]} {
		return "/dev/uart"
	} else {
		return "TCP4-LISTEN:5555"
	}
}

install_config {
config
+ parent-provides
  + service LOG
  + service PD
  + service CPU
  + service ROM
  + service IRQ
  + service IO_MEM
  + service IO_PORT
  + service RM

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 100 | ram: 1M

+ start timer
  + provides | + service Timer

} [uart_start_node] {
} [nic_start_nodes] {

+ start terminal_monitor | ram: 2M
  + binary terminal_crosslink
  + provides | + service Terminal

+ start terminal_gdb | ram: 2M
  + binary terminal_crosslink
  + provides | + service Terminal

+ start monitor | caps: 1000 | ram: 100M
  + config
  | + parent-provides
  |   + service LOG
  |   + service PD
  |   + service CPU
  |   + service ROM
  | + default | caps: 100
  | + monitor
  |   + policy | label: test-monitor_gdb | wait: yes | wx: yes
  |
  | + start test-monitor_gdb | caps: 300 | ram: 10M
  |   + config
  |     + vfs | + dir dev | + log
  |     + libc | stdout: /dev/log | stderr: /dev/log
  |   + route
  |     + service PD  | + local
  |     + service CPU | + local
  |     + any-service | + parent
  |
  | + start test-log | ram: 2M
  |   + route
  |     + service PD  | + local
  |     + service CPU | + local
  |     + any-service | + parent
  |
  + route
    + service Terminal | + child terminal_monitor
    + any-service
      + parent
      + any-child

+ start vfs | ram: 12M
  + provides | + service File_system
  + route | + any-service | + parent
  + config
    + default-policy | root: / | writeable: no
    + vfs
      + tar socat.tar
      + tar gdb.tar

+ start vfs_rom | ram: 60M
  + binary fs_rom
  + provides | + service ROM
  + config
  + route
    + service File_system | + child vfs
    + any-service         | + parent

+ start socat | caps: 200 | ram: 30M
  + binary /bin/socat
  + config
  | + vfs
  |   + dir dev
  |     + log
  |     + inline rtc | : 2018-01-01 00:01
  |     + terminal gdb | label: gdb
  |     } [socat_uart_dev] {
  |   + dir socket | } [socat_tcpip] {
  |   + fs
  | + libc | stdout: /dev/log | stderr: /dev/log | rtc: /dev/rtc | socket: /socket
  | + arg socat
  | + arg /dev/gdb
  | + arg } [socat_last_arg] {
  + route
    + service File_system                  | + child vfs
    + service ROM | label_suffix: .lib.so  | + parent
    + service ROM | label_last: /bin/socat | + child vfs_rom
    + service Terminal | label: uart       | + child uart
    + service Terminal | label: gdb        | + child terminal_gdb
    + any-service
      + parent
      + any-child

+ start gdb | caps: 300 | ram: 46M
  + binary /bin/} [gdb_prefix] {gdb
  + config | ld_verbose: yes
  | + vfs
  |   + dir dev
  |     + log
  |     + inline rtc | : 2023-01-01 00:01
  |     + terminal tty | label: tty
  |     + terminal monitor | label: monitor
  |   + dir pipe | + pipe
  |   + tar gdb_run_debug.tar
  |   + fs
  | + libc | stdin: /dev/tty | stdout: /dev/tty | stderr: /dev/tty
  |        | pipe: /pipe     | rtc: /dev/rtc
  | + arg gdb
  | + arg debug/ld.lib.so
  | + arg -ex | : set pagination off
  | + arg -ex | : set style enabled off
  | + arg -ex | : set interactive-mode off
  | + arg -ex | : set solib-search-path debug
  | + arg -ex | : set non-stop on
  | + arg -ex | : target extended-remote /dev/monitor
  | + arg -ex | : b binary_ready_hook_for_gdb
  | + arg -ex | : c
  | + arg -ex | : delete 1
  | + arg -ex | : thread 2
  | + arg -ex | : file debug/test-monitor_gdb
  + route
    + service ROM | label_last: /bin/} [gdb_prefix] {gdb | + child vfs_rom
    + service Terminal | label: tty                      | + child terminal_gdb
    + service Terminal | label: monitor                  | + child terminal_monitor
    + any-service
      + parent
      + any-child
-
}

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
