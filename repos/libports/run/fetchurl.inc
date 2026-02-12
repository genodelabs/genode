assert {![have_board rpi3]}
assert {![have_board imx53_qsb_tz]}
assert {![have_board rpi]}

assert {[have_recipe pkg/[drivers_nic_pkg]]} \
	"Recipe for 'pkg/[drivers_nic_pkg]' not available."

if {[have_cmd_switch --autopilot]} {
	assert {![have_board linux] && ![have_board virt_qemu_riscv]} \
	       "Autopilot mode is not supported on this platform."
}

#
# This run script works on Linux with NAT setup from tap0 to uplink
# device uplink0 like follows.
#
# iptables -t nat -A POSTROUTING -o uplink0 -j MASQUERADE
# iptables -A FORWARD -i tap0    -o uplink0 -j ACCEPT
# iptables -A FORWARD -i uplink0 -o tap0    -m state --state RELATED,ESTABLISHED -j ACCEPT
# echo 1 > /proc/sys/net/ipv4/ip_forward
#

proc socket_fs_plugin { } {
	set result "[ip_stack]"

	if {[have_board linux]} {
		append result { | ip_addr: 10.0.2.55 | netmask: 255.255.255.0}
		append result { | gateway: 10.0.2.1  | nameserver: 1.1.1.1}
	} else {
		append result { | dhcp: yes}
	}

	return $result
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg] \
                  [depot_user]/src/curl \
                  [depot_user]/src/fetchurl \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/nic_router \
                  [depot_user]/src/libssh \
                  [depot_user]/src/openssl \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_[ip_stack] \
                  [depot_user]/src/vfs_jitterentropy \
                  [depot_user]/src/vfs_pipe \
                  [depot_user]/src/zlib

install_config {
config
+ parent-provides
  + service CPU
  + service IO_MEM
  + service IO_PORT
  + service IRQ
  + service LOG
  + service PD
  + service RM
  + service ROM

+ default | caps: 100 | ram: 1M

+ default-route
  + service Report | + child report_rom
  + any-service
    + parent
    + any-child

+ start timer
  + provides | + service Timer

+ start drivers | caps: 1200 | ram: 32M | managing_system: yes
  + binary init
  + route
    + service Uplink              | + child nic_router
    + service ROM | label: config | + parent | label: drivers.config
    + service Timer               | + child timer
    + any-service                 | + parent

+ start nic_router | caps: 200 | ram: 10M
  + provides
    + service Nic
    + service Uplink
  + config | verbose_domain_state: yes
    + policy | label_prefix: fetchurl | domain: downlink
    + policy | label_prefix: drivers  | domain: uplink
    + domain uplink
      + nat | domain: downlink | tcp-ports: 16384 | udp-ports: 16384 | icmp-ids: 16384
    + domain downlink | interface: 10.0.3.1/24
      + dhcp-server | ip_first: 10.0.3.2 | ip_last: 10.0.3.2 | dns_config_from: uplink
      + tcp  | dst: 0.0.0.0/0 | + permit-any | domain: uplink
      + udp  | dst: 0.0.0.0/0 | + permit-any | domain: uplink
      + icmp | dst: 0.0.0.0/0 | domain: uplink

+ start report_rom | ram: 4M
  + provides
    + service ROM
    + service Report
  + config | verbose: yes

+ start fetchurl | caps: 500 | ram: 32M
  + config
    + report | progress: yes
    + vfs
    | + dir dev
    |   + log
    |   + null
    |   + inline rtc    | : 2000-01-01 00:00
    |   + inline random | : 0123456789012345678901234567890123456789
    | + dir pipe | + pipe
    | + dir socket | + } [socket_fs_plugin] {
    + libc | stdout: /dev/log    | stderr: /dev/log | rtc:    /dev/rtc
    |      | rng:    /dev/random | socket: /socket  | pipe:   /pipe
    + fetch | url: https://genode.org/about/LICENSE | path: /dev/log | retry: 3
-
}

build_boot_image [build_artifacts]

append qemu_args " -nographic "
append_qemu_nic_args

## Uncomment to dump network traffic to file
# append qemu_args " -object filter-dump,id=net0,netdev=net0,file=[run_dir].pcap"

run_genode_until {child "fetchurl" exited with exit value 0} 120
