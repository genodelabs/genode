assert_spec nova

if {[is_qemu_available]} {
	puts "\nScript running on Qemu is not supported.\n"
	exit
}

set build_components {
	core init virtualbox 
	server/part_blk
	server/rump_fs
	drivers/ahci
	drivers/input
	drivers/framebuffer
	drivers/timer
}

lappend_if [have_spec acpi]  build_components drivers/acpi
lappend_if [have_spec pci]   build_components drivers/pci

build $build_components

create_boot_directory

set config {
<config verbose="yes">
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
		<any-service><parent/><any-child/></any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="ahci">
		<binary name="ahci" />
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<service name="IRQ"><child name="acpi" /></service>
			<any-service> <parent /> <any-child /></any-service>
		</route>
	</start>
	<start name="part_blk">
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<any-service><child name="ahci"/> <parent/><any-child/></any-service>
		</route>
		<config>
			<policy label="rump_fs" partition="4"/>
		</config>
	</start>
	<start name="rump_fs">
		<resource name="RAM" quantum="32M" />
		<provides><service name="File_system"/></provides>
		<config fs="ext2fs"><policy label="" root="/" writeable="yes"/></config>
		<route>
			<any-service><child name="part_blk"/> <parent/><any-child/></any-service>
		</route>
	</start>
}

append_if [have_spec acpi] config {
	<start name="acpi">
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
	<start name="pci_drv">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="PCI"/> </provides>
	</start> }

append_if [have_spec ps2] config {
	<start name="ps2_drv">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
	</start> }

append_if [have_spec framebuffer] config {
	<start name="fb_drv">
		<resource name="RAM" quantum="6M"/>
		<provides><service name="Framebuffer"/></provides>
		<config buffered="yes" width="1400" height="1050" depth="16" />
	</start>}

append config $config_of_app

append config {
</config>
}

install_config $config

set boot_modules {
	core init timer
	part_blk ahci
	ld.lib.so libc.lib.so libc_fs.lib.so
	rump.lib.so rump_fs.lib.so rump_fs
	virtualbox vbox-auto-test-helper
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

# platform-specific modules
lappend_if [have_spec         ps2] boot_modules ps2_drv
lappend_if [have_spec        acpi] boot_modules acpi_drv
lappend_if [have_spec         pci] boot_modules pci_drv
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec        nova] boot_modules pci_device_pd

build_boot_image $boot_modules

append qemu_args " -m 1512 "
append qemu_args " -cpu phenom "
