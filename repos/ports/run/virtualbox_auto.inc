assert_spec nova

if {[is_qemu_available]} {
	puts "\nRun script does not support Qemu.\n"
	exit
}

append build_components {
	core init virtualbox 
	server/part_blk
	server/rump_fs
	drivers/ahci
	drivers/input
	drivers/framebuffer
	drivers/timer
}

lappend_if [have_spec acpi]   build_components drivers/acpi
lappend_if [have_spec pci]    build_components drivers/pci
lappend_if [have_spec x86]    build_components drivers/rtc
lappend_if [expr $use_ram_fs] build_components server/ram_fs

build $build_components

create_boot_directory

set config {
<config verbose="yes" prio_levels="4">
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="CAP"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service> <any-child/> <parent/> </any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="ahci" priority="-1">
		<binary name="ahci" />
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<service name="IRQ"><child name="acpi" /></service>
			<any-service> <parent /> <any-child /></any-service>
		</route>
	</start>
	<start name="part_blk" priority="-1">
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<any-service><child name="ahci"/> <parent/><any-child/></any-service>
		</route>
		<config>
			<policy label="rump_fs" partition="4"/>
		</config>
	</start>
	<start name="rump_fs" priority="-1">
		<resource name="RAM" quantum="32M" />
		<provides><service name="File_system"/></provides>
		<config fs="ext2fs"><policy label="" root="/" writeable="yes"/></config>
		<route>
			<any-service><child name="part_blk"/> <parent/><any-child/></any-service>
		</route>
	</start>}

append_if [expr $use_ram_fs] config {
	<start name="ram_fs" priority="-1">
		<resource name="RAM" quantum="512M" />
		<provides><service name="File_system"/></provides>
		<config>
			<content>
				<dir name="ram">
					<rom name="overlay.vdi"/>
				</dir>
			</content>
			<!-- constrain sessions according to their labels -->
			<policy label="vbox -> from_ram_fs" root="/ram" writeable="yes" />
		</config>
	</start>}

append_if [have_spec acpi] config {
	<start name="acpi" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if [expr ![have_spec acpi] && [have_spec pci]] config {
	<start name="pci_drv" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="PCI"/> </provides>
	</start>}

append_if [have_spec ps2] config {
	<start name="ps2_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
	</start>}

append_if [have_spec framebuffer] config {
	<start name="fb_drv" priority="-1">
		<resource name="RAM" quantum="6M"/>
		<provides><service name="Framebuffer"/></provides>
		<config buffered="yes" width="1024" height="768" depth="16" />
	</start>}

append_if [have_spec x86] config {
	<start name="rtc_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides>
			<service name="Rtc"/>
		</provides>
	</start>}

append config $config_of_app

append config {
</config>
}

install_config $config

append boot_modules {
	core init timer
	part_blk ahci
	ld.lib.so libc.lib.so libm.lib.so pthread.lib.so
	libc_lock_pipe.lib.so libc_terminal.lib.so
	libiconv.lib.so stdcxx.lib.so
	rump.lib.so rump_fs.lib.so rump_fs
	virtualbox
}

#
# Stuff to generate overlay.vdi if required
#
# generate overlay VDI
#proc vdi         { } {
#	global vdi_image
#	return "bin/$vdi_image"
#}
#proc overlay_vdi { } { return "bin/overlay.vdi" }
#proc vdi_size {} {
#	catch {
#		set result [ exec vboxmanage showhdinfo [vdi] | \
#		                  grep "^Logical" | sed "s/\[^0-9\]\\+//;s/ .*//" ]
#	}
#	return $result
#}
#
#file delete -force [overlay_vdi]
#if {$use_vdi} {
#	if {![file exists [vdi]]} {
#		puts "VDI image [vdi] missing!"
#		exit -1
#	}
#	catch {
#		exec -ignorestderr vboxmanage createhd --filename [overlay_vdi] --size [vdi_size] --format vdi
#		exec chmod a+r [overlay_vdi]
#		append boot_modules "overlay.vdi"
#	}
#}

lappend_if [expr $use_ram_fs]      boot_modules ram_fs
lappend_if [expr $use_ram_fs]      boot_modules overlay.vdi

# platform-specific modules
lappend_if [have_spec         ps2] boot_modules ps2_drv
lappend_if [have_spec        acpi] boot_modules acpi_drv
lappend_if [have_spec         pci] boot_modules pci_drv
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec        nova] boot_modules pci_device_pd
lappend_if [have_spec         x86] boot_modules rtc_drv
