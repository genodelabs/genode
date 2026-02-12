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

proc client_start { name prot ip_addr gateway netmask nic srv_port srv_ip } {
	global nr_of_clients
	incr nr_of_clients

	if {$ip_addr == "dhcp"} {
		append vfs_ip {lwip | dhcp: yes}
	} else {
		append vfs_ip {lwip | ip_addr: } $ip_addr \
		                  { | gateway: } $gateway \
		                  { | netmask: } $netmask
	}

	append result {
		+ start } $name { | priority: -1 | ram: 8M
		  + binary } [client_bin $prot] {
		  + route
		  | + service Nic | + child } $nic {
		  | + any-service
		  |   + parent
		  |   + any-child
		  + config | server_ip: } $srv_ip { | server_port: } $srv_port {
		    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
		    + vfs
		      + dir dev    | + log
		      + dir socket | + } $vfs_ip {
	}

	# strip tabs used for indentation
	set result [string map {"\t" ""} $result]

	return $result
}

proc server_start { name prot ip_addr gateway netmask nic port } {
	append result {
		+ start } $name { | priority: -1 | ram: 8M
		  + binary } [server_bin $prot] {
		  + route
		  | + service Nic | + child } $nic {
		  | + any-service
		  |   + parent
		  |   + any-child
		  + config | port: } $port {
		    + libc | stdout: /dev/log | stderr: /dev/log | socket: /socket
		    + vfs
		      + dir dev    | + log
		      + dir socket | + lwip | ip_addr: } $ip_addr {
		                              gateway: } $gateway {
		                              netmask: } $netmask {
	}

	# strip tabs used for indentation
	set result [string map {"\t" ""} $result]

	return $result
}

append qemu_args "-nographic "
