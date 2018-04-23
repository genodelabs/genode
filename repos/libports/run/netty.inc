assert_spec x86

proc use_dynamic_rom { } { return true }

set build_components {
	core init
	drivers/timer drivers/nic server/vfs
	lib/vfs/lxip
}

lappend_if [use_dynamic_rom] build_components server/dynamic_rom

source ${genode_dir}/repos/base/run/platform_drv.inc
append_platform_drv_build_components

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
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
}

append_platform_drv_config

append config {
	<start name="nic_drv">
		<binary name="} [nic_drv_binary] {"/>
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="Nic"/> </provides>
	</start>}

append_if [use_dynamic_rom] config {
	<start name="dynamic_rom">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="socket_fs.config"> <inline description="MTU default">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<lxip ip_addr="10.0.2.55" netmask="255.255.255.0" gateway="10.0.2.1" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/> <inline description="MTU 400">
			<config ld_verbose="yes">
				<vfs>
					<dir name="socket">
						<lxip mtu="400" ip_addr="10.0.2.55" netmask="255.255.255.0" gateway="10.0.2.1" nameserver="8.8.8.8"/>
					</dir>
				</vfs>
				<default-policy root="/socket" writeable="yes" />
			</config>
			</inline>
			<sleep milliseconds="3000"/>
			</rom>
		</config>
	</start>}

append config {
	<start name="socket_fs" caps="200">
		<binary name="vfs"/>
		<resource name="RAM" quantum="32M"/>
		<provides> <service name="File_system"/> </provides>}

if {[use_dynamic_rom]} { append config {
		<configfile name="socket_fs.config"/>
		<route>
			<service name="ROM" label="socket_fs.config"> <child name="dynamic_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>}
} else { append config {
		<config ld_verbose="yes">
			<vfs>
				<dir name="socket">
					<lxip ip_addr="10.0.2.55" netmask="255.255.255.0" gateway="10.0.2.1" nameserver="8.8.8.8"/>
					<!-- <lxip dhcp="yes"/> -->
				</dir>
			</vfs>
			<default-policy root="/socket" writeable="yes" />
		</config>}
}

append config {
	</start>
}

append boot_modules {
	core init timer } [nic_drv_binary] { vfs
	ld.lib.so libc.lib.so vfs.lib.so vfs_lxip.lib.so lxip.lib.so
}

lappend_if [use_dynamic_rom] boot_modules dynamic_rom

append_platform_drv_boot_modules

append qemu_args " -nographic -net nic,model=e1000 -net tap,ifname=tap0,downscript=no,script=no "

# vi: set ft=tcl :
