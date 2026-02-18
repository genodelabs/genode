# libc sockopt test
#
# uses the following config hook.
#
# proc ipstack { } { return <ip stack> }

set build_components {
	lib/ld core init timer
	lib/libc lib/vfs
	server/nic_router
	server/dynamic_rom
	test/sockopt

	lib/vfs_lxip
	lib/vfs_lwip
}

build $build_components

create_boot_directory

import_from_depot [depot_user]/src/vfs_lwip \
                  [depot_user]/src/vfs_lxip

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
  + config | verbose_domain_state: yes | verbose_packets: yes | verbose_packet_drop:  no
  | + default-policy | domain: default
  | + domain default | interface: 10.0.2.1/24
  |   + dhcp-server | ip_first: 10.0.2.2 | ip_last: 10.0.2.2
  |   + udp | dst: 0.0.0.0/0 | + permit-any | domain: default
  |   + tcp | dst: 0.0.0.0/0 | + permit-any | domain: default

+ start dynamic_rom | caps: 100 | ram: 1M
  + provides | + service ROM
  + config
  | + rom server.config
  |   + inline
  |   | + config
  |   |   + parent-provides
  |   |     + service CPU
  |   |     + service LOG
  |   |     + service Nic
  |   |     + service PD
  |   |     + service ROM
  |   |     + service Timer
  |   |   + start test-sockopt_server | caps: 200 | ram: 10M
  |   |     + config
  |   |     | + vfs
  |   |     |   + dir dev
  |   |     |     + log
  |   |     |     + null
  |   |     |   + dir socket
  |   |     |     + } [ipstack] {
  |   |     |       | ip_addr:    10.0.2.3
  |   |     |       | netmask:    255.255.255.0
  |   |     |       | gateway:    10.0.2.1
  |   |     |       | nameserver: 8.8.8.8
  |   |     | + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
  |   |     + route
  |   |       + any-service
  |   |         + parent
  |   + sleep | milliseconds: 15000 | . kill server after 15s
  |   + inline | + config
  |   + sleep | milliseconds: 3600000
  + route
    + any-service
      + parent
      + any-child

+ start server | caps: 300 | ram: 12M
  + binary init
  + route
    + service ROM   | label: config | + child dynamic_rom | label: server.config
    + service Nic                   | + child nic_router
    + service Timer                 | + child timer
    + any-service
      + parent
      + any-child

+ start test-sockopt_client | caps: 200 | ram: 10M
  + config | server_ip: 10.0.2.3
  | + vfs
  |   + dir dev
  |     + log
  |     + null
  |   + dir socket
  |     + } [ipstack] { | dhcp: yes
  | + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
  + route
    + service Nic | + child nic_router
    + any-service
      + parent
      + any-child
-}

install_config $config

build_boot_image [build_artifacts]

append qemu_args " -nographic "

run_genode_until {\[init -\> server -\> test-sockopt_server\] \[10\] accept ... \[ok\]} 60

set run_id [output_spawn_id]

# keep alive (timeout is set to 5s in test)
run_genode_until {\[init -\> nic_router].*snd ETH .*\> .* IPV4 .* \> .*TCP \d\d\d\d\d \> 80 flags 'a'} 8 $run_id

# ack keep alive
run_genode_until {\[init -\> nic_router].*snd ETH .*\> .* IPV4 .* \> .*TCP 80 \> \d\d\d\d\d flags 'a'} 2 $run_id

# wait for server to be shutdown
run_genode_until {\[init -\> nic_router] \[default] NIC sessions: 1} 15 $run_id

# scan for 2 keep alive packets with keepintvl 1s
run_genode_until {\[init -\> nic_router].*rcv ETH .*\> .* IPV4 .* \> .*TCP \d\d\d\d\d \> 80 flags 'a'} 10 $run_id
run_genode_until {\[init -\> nic_router].*rcv ETH .*\> .* IPV4 .* \> .*TCP \d\d\d\d\d \> 80 flags 'a'} 10 $run_id

# wait for packet with rst flag set (keepcnt is set to 2 in test)
run_genode_until {\[init -\> nic_router].*rcv ETH .*\> .* IPV4 .* \> .*TCP \d\d\d\d\d \> 80 flags 'ra'} 10 $run_id
