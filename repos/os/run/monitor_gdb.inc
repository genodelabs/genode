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

assert {[platform_supported]}

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
}

if { [have_include power_on/qemu] } {

	append_if [have_board pc] config {
		+ start terminal | ram: 2M
		  + binary pc_uart
		  + provides | + service Terminal
		  + config | + policy | label_prefix: monitor | uart: 1
	}

	append_if [have_board rpi3] config {
		+ start terminal | ram: 2M
		  + binary rpi3_uart
		  + provides | + service Terminal
		  + config | + policy | label_prefix: monitor | uart: 0
	}

} else {

	append config {
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
		    + policy | label_prefix: terminal | domain: downlink
		    + policy | label_prefix: drivers | domain: uplink
		    + domain uplink
		      + nat | domain: downlink | tcp-ports: 1 | udp-ports: 1 | icmp-ids: 1
		      + tcp-forward | port: 5555 | domain: downlink | to: 10.0.3.2
		    + domain downlink | interface: 10.0.3.1/24
		      + dhcp-server | ip_first: 10.0.3.2 | ip_last: 10.0.3.2
		      + tcp | dst: 0.0.0.0/0 | + permit-any | domain: uplink
		
		+ start terminal | caps: 200 | ram: 8M
		  + binary tcp_terminal
		  + provides | + service Terminal
		  + config
		  | + policy | label_prefix: monitor | port: 5555
		  | + vfs
		  |   + dir dev    | + log
		  |   + dir socket | + lwip | dhcp: yes
		  |   + dir pipe   | + pipe
		  | + libc | stdout: /dev/log | socket: /socket | pipe: /pipe
		  + route
		    + service Nic | + child nic_router
		    + any-service
		      + parent
		      + any-child
	}
}

# strip tabs used for indentation
set config [string map {"\t" ""} $config]

append config {
+ start monitor | caps: 1000 | ram: 100M
  + config
    + parent-provides
      + service LOG
      + service PD
      + service CPU
      + service ROM
    + default | caps: 100
    + monitor | + policy | label: test-monitor_gdb | wait: yes | wx: yes
    + start test-monitor_gdb | caps: 300 | ram: 10M
    | + config
    |   + vfs | + dir dev | + log
    |   + libc | stdout: /dev/log | stderr: /dev/log
    | + route
    |   + service PD  | + local
    |   + service CPU | + local
    |   + any-service | + parent
    |
    + start test-log | ram: 2M
      + route
        + service PD  | + local
        + service CPU | + local
        + any-service | + parent
-
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

	append qemu_args " -chardev socket,id=uart,port=$port,host=$host,server=on,wait=off,ipv4=on "

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
