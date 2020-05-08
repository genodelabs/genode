proc use_dynamic_rom { } { return true }

create_boot_directory
import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_nic_pkg] \
                  [depot_user]/src/dynamic_rom \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/vfs_audit \
                  [depot_user]/src/vfs_lxip \
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

	<start name="drivers" caps="1000">
		<resource name="RAM" quantum="32M" constrain_phys="yes"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<provides> <service name="Nic"/> </provides>
	</start> }

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
						<lxip mtu="400" ip_addr="10.0.2.55" netmask="255.255.255.0" nameserver="8.8.8.8"/>
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
		<route>
			<service name="ROM" label="config">
				<child name="dynamic_rom" label="socket_fs.config"/> </service>
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

proc qemu_nic_model {} {
	if [have_spec x86]         { return e1000 }
	if [have_spec lan9118]     { return lan9118 }
	if [have_spec zynq]        { return cadence_gem }
	return nic_model_missing
}
append qemu_args " -nographic "
append qemu_args " -net nic,model=[qemu_nic_model] "
append qemu_args " -net tap,ifname=tap0,downscript=no,script=no "

# vi: set ft=tcl :
