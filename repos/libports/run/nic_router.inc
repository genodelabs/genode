if {[have_cmd_switch --autopilot]} {
	assert {![have_board virt_qemu_riscv]} \
		"Autopilot mode is not supported on this platform."
}

set targets {
	core lib/ld init timer lib/libc lib/vfs lib/vfs_lwip
	server/report_rom server/nic_router server/nic_bridge server/nic_loopback
	test/lwip/http_srv test/lwip/http_clnt test/lwip/udp
}

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
		<start name="} $name {" priority="-1" ram="8M">
			<binary  name="} [client_bin $prot] {" />
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
		<start name="} $name {" priority="-1" ram="8M">
			<binary  name="} [server_bin $prot] {" />
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

append qemu_args "-nographic "
