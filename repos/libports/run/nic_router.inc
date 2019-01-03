
source ${genode_dir}/repos/base/run/platform_drv.inc

set targets "core init timer server/nic_router server/nic_bridge
             test/lwip/http_srv test/lwip/http_clnt test/lwip/udp
             server/nic_loopback lib/vfs/lwip [platform_drv_build_components]"

proc client_bin { prot } {
	if {$prot == "udp"}  { return "test-lwip-udp-client" }
	if {$prot == "http"} { return "test-http_clnt" } }

proc server_bin { prot } {
	if {$prot == "udp"}  { return "test-lwip-udp-server" }
	if {$prot == "http"} { return "test-lwip_httpsrv" } }

proc client_config { name prot ip_addr gateway netmask nic srv_port srv_ip } {
	global nr_of_clients
	incr nr_of_clients
	append result {
		<start name="} $name {" priority="-1">
			<binary  name="} [client_bin $prot] {" />
			<resource name="RAM" quantum="4M"/>
			<route>
				<service name="Nic"> <child name="} $nic {"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
			<config server_ip="} $srv_ip {" server_port="} $srv_port {">
				<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
				<vfs>
					<dir name="dev"> <log/> </dir>
					<dir name="socket">
	}
	if {$ip_addr == "dhcp"} {
		append result {
						<lwip dhcp="yes"/>}
	} else {
		append result {
						<lwip ip_addr="} $ip_addr {" gateway="} $gateway {" netmask="} $netmask {"/>}
	}
	append result {
					</dir>
				</vfs>
			</config>
		</start>}
	return $result
}

proc server_config { name prot ip_addr gateway netmask nic port } {
	append result {
		<start name="} $name {" priority="-1">
			<binary  name="} [server_bin $prot] {" />
			<resource name="RAM" quantum="4M"/>
			<route>
				<service name="Nic"> <child name="} $nic {"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
			<config port="} $port {">
				<vfs>
					<dir name="dev"> <log/> </dir>
					<dir name="socket">
						<lwip ip_addr="} $ip_addr {" gateway="} $gateway {" netmask="} $netmask {"/>
					</dir>
				</vfs>
				<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"
				      ip_addr="} $ip_addr {" gateway="} $gateway {" netmask="} $netmask {"/>
			</config>
		</start>}
	return $result
}

set boot_modules "
	core init timer nic_router nic_bridge nic_loopback ld.lib.so
	libc.lib.so vfs.lib.so vfs_lwip.lib.so [client_bin udp] [server_bin udp]
	[client_bin http] [server_bin http] [platform_drv_boot_modules]"

append qemu_args "-nographic "
