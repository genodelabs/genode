#
# \brief  Test of fetchurl
# \author Emery Hemingway
# \date   2016-06-05
#

if {[have_board rpi3] || [have_board imx53_qsb_tz] || [have_board rpi]} {
	puts "Run script does not support this platform."
	exit 0
}

if {[get_cmd_switch --autopilot] && ([have_board linux] ||
                                     [have_board virt_qemu_riscv])} {
	puts "Autopilot mode is not supported on this platform."
	exit 0
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
	set result "<[ip_stack]"

	if {[have_board linux]} {
		append result { ip_addr="10.0.2.55" netmask="255.255.255.0"}
		append result { gateway="10.0.2.1" nameserver="1.1.1.1"}
	} else {
		append result { dhcp="yes"}
	}

	append result {/>}
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
                  [depot_user]/src/zlib

install_config {
<config>
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
	<default caps="100"/>
	<default-route>
		<service name="Report"> <child name="report_rom"/> </service>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="drivers" caps="1200" managing_system="yes">
		<resource name="RAM" quantum="32M"/>
		<binary name="init"/>
		<route>
			<service name="Uplink"> <child name="nic_router"/> </service>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
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

			<policy label_prefix="fetchurl" domain="downlink"/>
			<policy label_prefix="drivers"  domain="uplink"/>

			<domain name="uplink">

				<nat domain="downlink"
				     tcp-ports="16384"
				     udp-ports="16384"
				     icmp-ids="16384"/>

			</domain>

			<domain name="downlink" interface="10.0.3.1/24">

				<dhcp-server ip_first="10.0.3.2" ip_last="10.0.3.2" dns_config_from="uplink"/>

				<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>
				<udp dst="0.0.0.0/0"><permit-any domain="uplink" /></udp>
				<icmp dst="0.0.0.0/0" domain="uplink"/>

			</domain>

		</config>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="ROM"/> <service name="Report"/> </provides>
		<config verbose="yes"/>
	</start>

	<start name="fetchurl" caps="500">
		<resource name="RAM" quantum="32M"/>
		<config>
			<report progress="yes"/>
			<vfs>
				<dir name="dev">
					<log/> <null/> <inline name="rtc">2000-01-01 00:00</inline>
					<inline name="random">01234567890123456789</inline>
				</dir>
				<dir name="socket"> } [socket_fs_plugin] { </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc"
			      rng="/dev/random" socket="/socket"/>
			<fetch url="https://genode.org/about/LICENSE" path="/dev/log" retry="3"/>
		</config>
	</start>
</config>
}

#build            { }
build_boot_image [build_artifacts]

append qemu_args " -nographic "
append_qemu_nic_args

## Uncomment to dump network traffic to file
# append qemu_args " -object filter-dump,id=net0,netdev=net0,file=[run_dir].pcap"

run_genode_until {child "fetchurl" exited with exit value 0} 120
