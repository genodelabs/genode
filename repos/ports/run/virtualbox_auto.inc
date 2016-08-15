if {(![have_spec nova] && ![have_spec hw_x86_64_muen])} {
	puts "Platform is unsupported."
	exit 0
}

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit
}

append build_components {
	core init
	server/part_blk
	server/rump_fs
	server/fs_rom
	drivers/ahci
	drivers/framebuffer
	drivers/timer
}

lappend_if [expr $use_vbox4]  build_components virtualbox
lappend_if [expr $use_vbox5]  build_components virtualbox5
lappend_if [expr $use_ps2]    build_components drivers/input
lappend_if [expr $use_usb]    build_components drivers/usb
lappend_if [expr $use_usb]    build_components app/usb_report_filter
lappend_if [have_spec x86]    build_components drivers/rtc
lappend_if [expr $use_ram_fs || $use_usb] build_components server/ram_fs

source ${genode_dir}/repos/base/run/platform_drv.inc
# override defaults of platform_drv.inc
proc platform_drv_priority {} { return { priority="-1"} }

append_platform_drv_build_components

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
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>}

append_if [have_spec muen] config {
		<service name="VM"/>}

append config {
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="ahci_drv" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<config>
			<policy label="part_blk" device="0"/>
		</config>
		<route>
			<service name="IRQ"><child name="acpi_drv"/></service>
			<any-service> <parent/> <any-child/></any-service>
		</route>
	</start>
	<start name="part_blk" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<route>
			<any-service><child name="ahci_drv"/> <parent/><any-child/></any-service>
		</route>
		<config>
			<policy label="rump_fs" partition="4"/>
		</config>
	</start>
	<start name="rump_fs" priority="-1">
		<resource name="RAM" quantum="32M"/>
		<provides><service name="File_system"/></provides>
		<config fs="ext2fs">}
append_if [expr $use_ram_fs && $use_overlay_from_disk] config {
			<policy label="overlay_from_disk" root="/ram" writeable="yes"/>}
append config {
			<default-policy root="/" writeable="yes"/>
		</config>
		<route>
			<any-service><child name="part_blk"/> <parent/><any-child/></any-service>
		</route>
	</start>}

append_if [expr $use_ram_fs] config {
	<start name="ram_fs" priority="-1">
		<resource name="RAM" quantum="512M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<content>
				<dir name="ram"> }
append_if [expr $use_ram_fs] config "<rom name=\"${overlay_image}\"/>"
append_if [expr $use_ram_fs] config {
				</dir>
			</content>
			<!-- constrain sessions according to their labels -->
			<policy label="vbox -> from_ram_fs" root="/ram" writeable="yes"/>
		</config>
		<route>}
append_if [expr $use_ram_fs && $use_overlay_from_disk] config "<service name=\"ROM\" label=\"${overlay_image}\">"
append_if [expr $use_ram_fs && $use_overlay_from_disk] config {
			<child name="overlay_from_disk"/></service>}
append_if [expr $use_ram_fs] config {
			<any-service> <parent/> <any-child/></any-service>
		</route>
	</start>}

append_if [expr $use_ram_fs && $use_overlay_from_disk] config {
	<start name="overlay_from_disk" priority="-1">
		<binary name="fs_rom"/>
		<resource name="RAM" quantum="256M"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="File_system"><child name="rump_fs"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>}

append_platform_drv_config

append_if [expr $use_ps2] config {
	<start name="ps2_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
		<route>
			<service name="IRQ"><child name="acpi_drv"/></service>
			<any-service> <parent/> <any-child/></any-service>
		</route>
	</start>}

append_if [expr $use_usb] config {
	<start name="usb_config_fs" priority="-1">
		<binary name="ram_fs"/>
		<resource name="RAM" quantum="1M"/>
		<provides><service name="File_system"/></provides>
		<config verbose="yes">
			<content>
				<inline name="usb_drv.config">}

append_if [expr $use_usb && ![have_spec hw_x86_64_muen]] config {
<config uhci="yes" ehci="yes" xhci="yes">}

append_if [expr $use_usb && [have_spec hw_x86_64_muen]] config {
<config uhci="no" ehci="no" xhci="yes">}

append_if [expr $use_usb] config {
	<hid/>
	<raw>
		<report devices="yes"/>
	</raw>
</config>
				</inline>
				<inline name="usb_report_filter.config">
<config>
	<!-- USB device whitelist -->
	<client label="vbox"/>
	<device vendor_id="0x13fe" product_id="0x5200"/> <!-- platinum stick -->
	<device vendor_id="0x148f" product_id="0x2573"/> <!-- ralink wifi -->
	<device vendor_id="0x1f75" product_id="0x0917"/> <!-- intenso stick -->
	<device vendor_id="0x04f9" product_id="0x0051"/> <!-- brother printer -->
	<device vendor_id="0x090c" product_id="0x6000"/> <!-- hama sd/hc reader -->
	<device vendor_id="0x1b1c" product_id="0x1a09"/> <!-- voyager gt stick -->
	<device vendor_id="0x1b1c" product_id="0x1a03"/> <!-- voyager stick -->
	<device vendor_id="0x04b3" product_id="0x310d"/> <!-- lenovo mouse -->
	<device vendor_id="0x04b3" product_id="0x310c"/> <!-- lenovo mouse -->
	<device vendor_id="0x04b3" product_id="0x3025"/> <!-- lenovo keyboard -->
	<device vendor_id="0x046a" product_id="0x0001"/> <!-- cherry keyboard -->
	<device vendor_id="0x0482" product_id="0x0015"/> <!-- kyo printer -->
	<device vendor_id="0x04a9" product_id="0x2220"/> <!-- canon scanner -->
	<device vendor_id="0x067b" product_id="0x2303"/> <!-- prolific serial -->
	<device vendor_id="0x0583" product_id="0x2060"/> <!-- snes controller -->
	<device vendor_id="0x045e" product_id="0x028e"/> <!-- x360 controller -->
	<device vendor_id="0x046d" product_id="0xc00e"/> <!-- logitech mouse -->
	<device vendor_id="0x0984" product_id="0x0066"/> <!-- lenovo DVDRW -->
	<device vendor_id="0x174c" product_id="0x5106"/> <!-- delock SATA -->
</config>
				</inline>
			</content>
			<policy label_prefix="usb_report_filter" root="/" writeable="yes"/>
			<policy label_prefix="usb_config_rom" root="/"/>
		</config>
	</start>

	<start name="usb_config_rom" priority="-1">
		<binary name="fs_rom"/>
		<resource name="RAM" quantum="1200K"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="File_system"><child name="usb_config_fs"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>

	<start name="usb_report_filter" priority="-1">
		<resource name="RAM" quantum="1200K"/>
		<configfile name="usb_report_filter.config"/>
		<route>
			<service name="Report"><child name="report_rom"/></service>
			<service name="ROM" label="usb_report_filter.config">
				<child name="usb_config_rom"/>
			</service>
			<service name="ROM" label="devices">
				<child name="report_rom"/>
			</service>
			<service name="ROM" label="usb_drv_config">
				<child name="report_rom"/>
			</service>
			<service name="File_system" label="usb_drv.config">
				<child name="usb_config_fs"/>
			</service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>

	<start name="usb_drv" priority="-1">
		<resource name="RAM" quantum="16M"/>
		<provides>
			<service name="Input"/>
			<service name="Usb"/>
		</provides>
		<configfile name="usb_drv.config"/>
		<route>
			<service name="IRQ"><child name="acpi_drv"/></service>
			<service name="Report"><child name="report_rom"/></service>
			<service name="ROM" label="usb_drv.config"><child name="usb_config_rom"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>
}

append_if [have_spec framebuffer] config {
	<start name="fb_drv" priority="-1">
		<resource name="RAM" quantum="6M"/>
		<provides><service name="Framebuffer"/></provides>
		<config buffered="yes"/>
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
	part_blk ahci_drv fs_rom
	ld.lib.so libc.lib.so libm.lib.so pthread.lib.so
	libc_pipe.lib.so libc_terminal.lib.so
	libiconv.lib.so stdcxx.lib.so
	rump.lib.so rump_fs.lib.so rump_fs
	qemu-usb.lib.so
}

lappend_if [expr $use_vbox4] boot_modules virtualbox
lappend_if [expr $use_vbox5] boot_modules virtualbox5
lappend_if [expr $use_ram_fs || $use_usb] boot_modules ram_fs
lappend_if [expr $use_ram_fs && !$use_overlay_from_disk] boot_modules ${overlay_image}

# platform-specific modules
lappend_if [expr         $use_ps2] boot_modules ps2_drv
lappend_if [expr         $use_usb] boot_modules usb_drv
lappend_if [expr         $use_usb] boot_modules usb_report_filter
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec         x86] boot_modules rtc_drv

append_platform_drv_boot_modules
