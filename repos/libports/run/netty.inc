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
<config verbose="yes">

	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	</parent-provides>

	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

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
		<config verbose_domain_state="yes" verbose_packets="no">
			<policy label_prefix="socket_fs" domain="downlink"/>
			<policy label_prefix="drivers"   domain="uplink"/>

			<domain name="uplink" interface="10.0.2.55/24" gateway="10.0.2.1">

				<nat domain="downlink"
				     tcp-ports="16384"
				     udp-ports="16384"
				     icmp-ids="16384"/>

				<tcp-forward port="80"   domain="downlink" to="10.0.3.55"/>
				<tcp-forward port="8080" domain="downlink" to="10.0.3.55"/>

				<udp-forward port="7"    domain="downlink" to="10.0.3.55"/>
				<udp-forward port="7070" domain="downlink" to="10.0.3.55"/>

			</domain>

			<domain name="downlink" interface="10.0.3.1/24">

				<dhcp-server ip_first="10.0.3.55" ip_last="10.0.3.55">
					<dns-server ip="1.1.1.1"/>
					<dns-server ip="8.8.8.8"/>
				</dhcp-server>

				<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>
				<udp dst="0.0.0.0/0"><permit-any domain="uplink" /></udp>
				<icmp dst="0.0.0.0/0" domain="uplink"/>

			</domain>
		</config>
	</start>
}

append_if [use_dynamic_rom] config {
	<start name="dynamic_rom">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="socket_fs.config"> <inline description="MTU default">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<} [ipstack] { ip_addr="10.0.3.55" netmask="255.255.255.0" gateway="10.0.3.1" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/> <inline description="MTU 400">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<} [ipstack] { mtu="400" ip_addr="10.0.3.55" netmask="255.255.255.0" gateway="10.0.3.1" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/>
			</rom>
		</config>
	</start>
}

append config {
	<start name="socket_fs" caps="200">
		<binary name="vfs"/>
		<resource name="RAM" quantum="32M"/>
		<provides> <service name="File_system"/> </provides>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>}

if {[use_dynamic_rom]} { append config {

			<service name="ROM" label="config">
				<child name="dynamic_rom" label="socket_fs.config"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>}

} else { append config {

			<any-service> <parent/> <any-child/> </any-service>
		</route>
		<config ld_verbose="yes">
			<vfs>
				<dir name="socket">
					<!-- <} [ipstack] { ip_addr="10.0.3.55" netmask="255.255.255.0" gateway="10.0.3.1" nameserver="8.8.8.8"/> -->
					<} [ipstack] { dhcp="yes"/>
				</dir>
				<!-- change default policy to "/audit" for more verbosity -->
				<dir name="audit">
					<audit path="/socket"/>
				</dir>
			</vfs>
			<default-policy root="/socket" writeable="yes" />
		</config>}
}

append config {
	</start>
}

if {[protocol] == "tcp"} { append config {
	<start name="netty-server-80">
		<binary name="test-netty_tcp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" port="80" read_write="no" nonblock="true">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
	<start name="netty-server-8080">
		<binary name="test-netty_tcp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" mode="server" port="8080" nonblock="false">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
<!--
	<start name="netty-client-A">
		<binary name="test-netty_tcp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" mode="client" ip="10.0.3.1" port="80">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
-->}
} else { append config {
	<start name="netty-server-7">
		<binary name="test-netty_udp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" port="7" read_write="no" nonblock="true">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
	<start name="netty-server-7070">
		<binary name="test-netty_udp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" mode="server" port="7070" nonblock="false">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
<!--
	<start name="netty-client-A">
		<binary name="test-netty_udp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" mode="client" ip="10.0.3.1" port="7">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
-->}
}

append config {
</config>
}

install_config $config
build_boot_image [build_artifacts]

append qemu_args " -nographic "
append_qemu_nic_args "host=10.0.2.1,dhcpstart=10.0.2.55,hostfwd=tcp::10080-:80,hostfwd=tcp::18080-:8080,hostfwd=udp::10007-:7,hostfwd=udp::17070-:7070"

# this works only if board/*/qemu_args does not declare "-netdev user,..."
#append qemu_args " -netdev tap,id=net0,ifname=tap0,script=no "

run_genode_until forever

# vi: set ft=tcl :
