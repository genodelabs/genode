assert_spec nova

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit
}

append build_components {
	core init virtualbox 
	server/part_blk
	server/rump_fs
	drivers/ahci
	drivers/framebuffer
	drivers/timer
}

lappend_if [expr $use_ps2]    build_components drivers/input
lappend_if [expr $use_usb]    build_components drivers/usb
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
		<any-service> <parent/> <any-child/> </any-service>
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
				<dir name="ram"> }
append_if [expr $use_ram_fs] config "<rom name=\"${overlay_image}\"/>"
append_if [expr $use_ram_fs] config {
				</dir>
			</content>
			<!-- constrain sessions according to their labels -->
			<policy label="vbox -> from_ram_fs" root="/ram" writeable="yes" />
		</config>
	</start>}

append_if [have_spec acpi] config {
	<start name="acpi" priority="-1">
		<resource name="RAM" quantum="12M" constrain_phys="yes"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<config>
			<policy label="acpi_drv"> <pci class="ALL"/> </policy>
			<policy label="ahci">     <pci class="AHCI"/> </policy>
			<policy label="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label="ps2_drv">  <device name="PS2"/> </policy>
			<policy label="usb_drv">  <pci class="USB"/> </policy>
			<policy label="fb_drv">   <pci class="VGA"/> </policy>
		</config>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if [expr ![have_spec acpi] && [have_spec pci]] config {
	<start name="pci_drv" priority="-1">
		<resource name="RAM" quantum="2M" constrain_phys="yes"/>
		<provides> <service name="PCI"/> </provides>
		<config>
			<policy label="ahci">     <pci class="AHCI"/> </policy>
			<policy label="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label="ps2_drv">  <device name="PS2"/> </policy>
			<policy label="usb_drv">  <pci class="USB"/> </policy>
			<policy label="fb_drv">   <pci class="VGA"/> </policy>
		</config>
	</start>}

append_if [expr $use_ps2] config {
	<start name="ps2_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
		<route>
			<service name="IRQ"><child name="acpi" /></service>
			<any-service> <parent /> <any-child /></any-service>
		</route>
	</start>}

append_if [expr $use_usb] config {
	<start name="usb_drv" priority="-1">
		<resource name="RAM" quantum="7M"/>
		<provides><service name="Input"/></provides>
		<config uhci="yes" ehci="yes" xhci="yes">
			<hid/>
		</config>
		<route>
			<service name="IRQ"><child name="acpi" /></service>
			<any-service> <parent /> <any-child /></any-service>
		</route>
	</start>
}

append_if [have_spec framebuffer] config {
	<start name="fb_drv" priority="-1">
		<resource name="RAM" quantum="6M"/>
		<provides><service name="Framebuffer"/></provides>
		<config buffered="yes" />
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

lappend_if [expr $use_ram_fs]      boot_modules ram_fs
lappend_if [expr $use_ram_fs]      boot_modules ${overlay_image}

# platform-specific modules
lappend_if [expr         $use_ps2] boot_modules ps2_drv
lappend_if [expr         $use_usb] boot_modules usb_drv
lappend_if [have_spec        acpi] boot_modules acpi_drv
lappend_if [have_spec         pci] boot_modules pci_drv
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec        nova] boot_modules pci_device_pd
lappend_if [have_spec         x86] boot_modules rtc_drv
