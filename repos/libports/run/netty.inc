# Network echo test
#
# uses the following config hooks.
#
# proc ipstack { }          name of IP stack (lwip or lxip)
# proc protocol { }         test protocol (tcp or udp)
# proc use_dynamic_rom { }  dynamically change IP-stack config at runtime

set build_components { }

if {[protocol] == "tcp"} {
	append build_components { test/netty/tcp }
} else {
	append build_components { test/netty/udp }
}

build $build_components

create_boot_directory
import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg] \
                  [depot_user]/src/dynamic_rom \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/nic_router \
                  [depot_user]/src/vfs_audit \
                  [depot_user]/src/vfs_[ipstack] \
                  [depot_user]/src/vfs

append config {
config | verbose: yes
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

+ default | caps: 100 | ram: 1M

+ start timer
  + provides | + service Timer

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
  + config | verbose_domain_state: yes | verbose_packets: no
    + policy | label_prefix: socket_fs | domain: downlink
    + policy | label_prefix: drivers   | domain: uplink
    + domain uplink | interface: 10.0.2.55/24 | gateway: 10.0.2.1
      + nat | domain: downlink | tcp-ports: 16384 | udp-ports: 16384 | icmp-ids: 16384
      + tcp-forward | port:   80 | domain: downlink | to: 10.0.3.55
      + tcp-forward | port: 8080 | domain: downlink | to: 10.0.3.55
      + udp-forward | port:    7 | domain: downlink | to: 10.0.3.55
      + udp-forward | port: 7070 | domain: downlink | to: 10.0.3.55
    + domain downlink | interface: 10.0.3.1/24
      + tcp  | dst: 0.0.0.0/0 | + permit-any | domain: uplink
      + udp  | dst: 0.0.0.0/0 | + permit-any | domain: uplink
      + icmp | dst: 0.0.0.0/0 | domain: uplink
      + dhcp-server | ip_first: 10.0.3.55 | ip_last: 10.0.3.55
        + dns-server | ip: 1.1.1.1
        + dns-server | ip: 8.8.8.8}

append_if [use_dynamic_rom] config {
+ start dynamic_rom | ram: 4M
  + provides | + service ROM
  + config | verbose: yes
    + rom socket_fs.config
      + inline | description: MTU default
      | + config | ld_verbose: yes
      |   + default-policy | root: /socket | writeable: yes
      |   + vfs
      |     + dir socket
      |       + } [ipstack] {
      |         ip_addr:    10.0.3.55
      |         netmask:    255.255.255.0
      |         gateway:    10.0.3.1
      |         nameserver: 8.8.8.8
      |
      + sleep | milliseconds: 3000
      + inline | description: MTU 400
      | + config | ld_verbose: yes
      |   + default-policy | root: /socket | writeable: yes
      |   + vfs
      |     + dir socket
      |       + } [ipstack] {
      |         mtu:        400
      |         ip_addr:    10.0.3.55
      |         netmask:    255.255.255.0
      |         gateway:    10.0.3.1
      |         nameserver: 8.8.8.8
      |
      + sleep | milliseconds: 3000}

append config {
+ start socket_fs | caps: 200 | ram: 32M
  + binary vfs
  + provides | + service File_system
  + route
    + service Nic | + child nic_router}
if {[use_dynamic_rom]} { append config {
    + service ROM | label: config
      + child dynamic_rom | label: socket_fs.config
    + any-service
      + parent
      + any-child}
} else { append config {
    + any-service
      + parent
      + any-child
  + config | ld_verbose: yes
    + default-policy | root: /socket | writeable: yes
    + vfs
      + dir socket
      | x } [ipstack] {
      |   ip_addr:    10.0.3.55
      |   netmask:    255.255.255.0
      |   gateway:    10.0.3.1
      |   nameserver: 8.8.8.8
      | + } [ipstack] { | dhcp: yes
      + dir audit
        . change default policy to "/audit" for more verbosity
        + audit | path: /socket}
}

if {[protocol] == "tcp"} { append config {
+ start netty-server-80 | ram: 4M
  + binary test-netty_tcp
  + config | ld_verbose: yes | port: 80 | read_write: no | nonblock: true
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram
+ start netty-server-8080 | ram: 4M
  + binary test-netty_tcp
  + config | ld_verbose: yes | mode: server | port: 8080 | nonblock: false
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram
x start netty-client-A | ram: 4M
  + binary test-netty_tcp
  + config | ld_verbose: yes | mode: client | ip: 10.0.3.1 | port: 80
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram}

} else { append config {
+ start netty-server-7 | ram: 4M
  + binary test-netty_udp
  + config | ld_verbose: yes | port: 7 | read_write: no | nonblock: true
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram
+ start netty-server-7070 | ram: 4M
  + binary test-netty_udp
  + config | ld_verbose: yes | mode: server | port: 7070 | nonblock: false
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram
x start netty-client-A | ram: 4M
  + binary test-netty_udp
  + config | ld_verbose: yes | mode: client | ip: 10.0.3.1 | port: 7
    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
    + vfs
      + dir dev    | + log
      + dir socket | + fs
      + dir tmp    | + ram}
}

append config {
-}

install_config $config
build_boot_image [build_artifacts]

append qemu_args " -nographic "
append_qemu_nic_args "host=10.0.2.1,dhcpstart=10.0.2.55,hostfwd=tcp::10080-:80,hostfwd=tcp::18080-:8080,hostfwd=udp::10007-:7,hostfwd=udp::17070-:7070"

# this works only if board/*/qemu_args does not declare "-netdev user,..."
#append qemu_args " -netdev tap,id=net0,ifname=tap0,script=no "

run_genode_until forever

# vi: set ft=tcl :
