# Low level IP stack test
#
# uses the following config hook.
#
# proc ipstack { }          name of IP stack (lwip or lxip)

set build_components {
	lib/ld core init timer
	server/nic_router
}

append build_components { test/ip_raw/} [ipstack]

build $build_components

create_boot_directory

append config {
config | verbose: yes
+ parent-provides
  + service CPU
  + service IO_MEM
  + service IO_PORT
  + service IRQ
  + service LOG
  + service PD
  + service RM
  + service ROM

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 100 | ram: 1M

+ start timer
  + provides | + service Timer

+ start nic_router | caps: 200 | ram: 10M
  + provides
    + service Nic
    + service Uplink
  + config
    |      verbose_domain_state: yes
    |      verbose_packets:      no
    |      verbose_packet_drop:  no
    |
    + default-policy | domain: default
    + domain default | interface: 10.0.2.1/24
      + dhcp-server | ip_first: 10.0.2.2 | ip_last: 10.0.2.2
      + udp | dst: 0.0.0.0/0 | + permit-any | domain: default
      + tcp | dst: 0.0.0.0/0 | + permit-any | domain: default

+ start test-} [ipstack] {_server | caps: 200 | ram: 10M
  + config
  |        ld_verbose: yes
  |        ip_addr:    10.0.2.3
  |        netmask:    255.255.255.0
  |        gateway:    10.0.2.1
  |        nameserver: 8.8.8.8
  + route
    + service Nic | + child nic_router
    + any-service
      + parent
      + any-child

+ start test-} [ipstack] {_client | caps: 200 | ram: 10M
  + config | ld_verbose: yes | server_ip: 10.0.2.3 | server_port: 80
  + route
    + service Nic | + child nic_router
    + any-service
      + parent
      + any-child
-
}

install_config $config

build_boot_image [build_artifacts]

append qemu_args " -nographic "

run_genode_until "test-[ipstack]_server\] Success" 10
