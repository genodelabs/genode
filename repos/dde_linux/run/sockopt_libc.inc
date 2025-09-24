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
<config verbose="yes">
	<parent-provides>
		<service name="CPU"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="IRQ"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="ROM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100" ram="1M"/>

	<start name="timer">
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="nic_router" caps="200" ram="10M">
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="yes" verbose_packets="yes" verbose_packet_drop="no">

			<default-policy domain="default"/>
			<domain name="default" interface="10.0.2.1/24">

				<dhcp-server ip_first="10.0.2.2" ip_last="10.0.2.2"/>
				<udp dst="0.0.0.0/0">
					<permit-any domain="default"/>
				</udp>
				<tcp dst="0.0.0.0/0">
					<permit-any domain="default"/>
				</tcp>

			</domain>
		</config>
	</start>

	<start name="dynamic_rom" caps="100" ram="1M">
		<provides> <service name="ROM"/> </provides>
		<config>
			<rom name="server.config">
				<inline>
					<config>
						<parent-provides>
							<service name="CPU"/>
							<service name="LOG"/>
							<service name="PD"/>
							<service name="ROM"/>
							<service name="Nic"/>
							<service name="Timer"/>
						</parent-provides>

						<start name="test-sockopt_server" caps="200" ram="10M">
							<config>
								<vfs>
									<dir name="dev"> <log/> <null/> </dir>
									<dir name="socket">
										<} [ipstack] { ip_addr="10.0.2.3" netmask="255.255.255.0" gateway="10.0.2.1" nameserver="8.8.8.8"/>
									</dir>
								</vfs>
								<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
							</config>
							<route>
								<any-service> <parent/> </any-service>
							</route>
						</start>
					</config>
				</inline>
				<!-- kill server after 15s -->
				<sleep milliseconds="15000"/>
				<inline> <config/> </inline>
				<sleep milliseconds="3600000"/>
			</rom>
		</config>
		<route>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="server" caps="300" ram="12M">
		<binary name="init"/>
		<route>
			<service name="ROM" label="config">
				<child name="dynamic_rom" label="server.config"/>
			</service>
			<service name="Nic"> <child name="nic_router"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="test-sockopt_client" caps="200" ram="10M">
		<config server_ip="10.0.2.3">
			<vfs>
				<dir name="dev"> <log/> <null/> </dir>
				<dir name="socket">
					<} [ipstack] { dhcp="yes"/>
				</dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
</config>
}

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
