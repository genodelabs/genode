
source ${genode_dir}/repos/base/run/platform_drv.inc

proc nic_drv { } {
	if {[nic_drv_binary] == "usb_drv"} { return usb_drv }
	if {[nic_drv_binary] == ""}        { return "" }
	return nic_drv
}

proc gpio_drv { } {
	if {[have_spec rpi] && [have_spec hw]}  { return hw_gpio_drv }
	if {[have_spec rpi] && [have_spec foc]} { return foc_gpio_drv }
	if {[have_spec gpio]}                   { return gpio_drv }
}

if {[nic_drv] == ""} {
	puts "\n Run script is not supported on this platform. \n"; exit 0 }

proc nic_drv_build { } {
	if {[nic_drv] == "nic_drv"} { return drivers/nic }
	if {[nic_drv] == "usb_drv"} { return drivers/usb }
}

proc gpio_drv_build { } { if {[gpio_drv] != ""} { return drivers/gpio } }

set targets "core init drivers/timer server/nic_router server/nic_bridge
             test/lwip/http_srv_static test/lwip/http_clnt test/lwip/udp
             [nic_drv_build] [gpio_drv_build] [platform_drv_build_components]"

proc gpio_drv_config { } {
	if {[have_spec gpio]} {
		append result {
			<start name="} [gpio_drv] {" caps="140" priority="-1">
				<resource name="RAM" quantum="4M"/>
				<provides><service name="Gpio"/></provides>
			</start>}
		return $result
	}
}

proc nic_drv_config { } {
	if {[nic_drv] == "nic_drv"} {
		append result {
			<start name="nic_drv" priority="-1">
				<binary name="} [nic_drv_binary] {"/>
				<resource name="RAM" quantum="6M"/>
				<provides><service name="Nic"/></provides>
			</start>}
		return $result
	}
	if {[nic_drv] == "usb_drv"} {
		append result {
			<start name="usb_drv" caps="120" priority="-1">
				<resource name="RAM" quantum="14M"/>
				<provides>
					<service name="Nic"/>
				</provides>
				<config ehci="yes">
					<nic mac="02:00:00:00:01:01" />
				</config>
			</start>}
		return $result
	}
}

proc client_bin { prot } {
	if {$prot == "udp"}  { return "test-lwip-udp-client" }
	if {$prot == "http"} { return "test-http_clnt" } }

proc server_bin { prot } {
	if {$prot == "udp"}  { return "test-lwip-udp-server" }
	if {$prot == "http"} { return "test-lwip_httpsrv_static" } }

proc client_config { prot index ip_addr gateway netmask nic srv_port srv_ip } {
	append result {
		<start name="} $prot {_client_} $index {" caps="200" priority="-1">
			<binary  name="} [client_bin $prot] {" />
			<resource name="RAM" quantum="10M"/>
			<route>
				<service name="Nic"> <child name="} $nic {"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
			<config server_ip="} $srv_ip {" server_port="} $srv_port {">
				<vfs> <dir name="dev"> <log/> </dir> </vfs>
				<libc stdout="/dev/log" stderr="/dev/log" ip_addr="} $ip_addr {"
				      gateway="} $gateway {" netmask="} $netmask {"/>
			</config>
		</start>}
	return $result
}

proc server_config { prot index ip_addr gateway netmask nic port } {
	append result {
		<start name="} $prot {_server_} $index {" caps="200" priority="-1">
			<binary  name="} [server_bin $prot] {" />
			<resource name="RAM" quantum="10M"/>
			<route>
				<service name="Nic"> <child name="} $nic {"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
			<config port="} $port {">
				<vfs> <dir name="dev"> <log/> </dir> </vfs>
				<libc stdout="/dev/log" stderr="/dev/log" ip_addr="} $ip_addr {"
				      gateway="} $gateway {" netmask="} $netmask {"/>
			</config>
		</start>}
	return $result
}

set boot_modules "
	core init timer nic_router nic_bridge ld.lib.so libc.lib.so libm.lib.so
	lwip.lib.so posix.lib.so [client_bin udp] [server_bin udp]
	[client_bin http] [server_bin http] [nic_drv_binary] [gpio_drv]
	[platform_drv_boot_modules]"

proc nic_qemu_args { } {
	if {[have_spec x86]}     { return "-net nic,model=e1000" }
	if {[have_spec lan9118]} { return "-net nic,model=lan9118" } }

append qemu_args "-nographic -net user [nic_qemu_args]"
