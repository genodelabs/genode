assert_spec x86

if {(![have_spec nova] && ![have_spec foc])} {
	puts "Platform is unsupported."
	exit 0
}

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit
}

if { [get_cmd_switch --autopilot] && [have_spec x86_32] } {
	puts "Run script does not support autopilot mode on 32 bit"
	exit 0
}

if {[have_spec nova]} {
	proc kernel_output { } { return "logmem" }
}

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/fs_rom \
                  [depot_user]/src/init \
                  [depot_user]/src/libc \
                  [depot_user]/src/libiconv \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/part_block \
                  [depot_user]/src/platform \
                  [depot_user]/src/acpi \
                  [depot_user]/src/pci_decode \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/stdcxx \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_pipe \
                  [depot_user]/src/vfs_import \
                  [depot_user]/src/jpeg

if {$use_vms > 1} {
	import_from_depot [depot_user]/pkg/themed_wm
}

if {$use_usb} {
	import_from_depot [depot_user]/src/pc_usb_host \
	                  [depot_user]/src/usb_hid
}

if {!$use_usb} {
	import_from_depot [depot_user]/src/black_hole
}

if {$use_rumpfs} {
	import_from_depot [depot_user]/src/vfs_rump
}

append build_components {
	driver/ahci
	driver/framebuffer
	driver/rtc
	driver/ps2
	lib/qemu-usb lib/libyuv
	virtualbox5
}

lappend_if [expr $use_serial]   build_components server/log_terminal
lappend_if [expr $use_ram_fs || $use_usb] build_components lib/vfs_import
lappend_if [expr $use_cpu_load] build_components app/trace_subject_reporter
lappend_if [expr $use_cpu_load] build_components app/cpu_load_display
lappend_if [expr $use_cpu_load] build_components app/top
lappend_if [have_spec nova]     build_components app/log_core

build $build_components

if {$use_rumpfs && !$use_ram_fs} {
	set rump_writeable true
} else {
	set rump_writeable false
}

set config {
<config verbose="yes" prio_levels="4">}

if {$use_vms > 1} {
	append config "
	<affinity-space width=\"${affinity_space_width}\" height=\"1\"/>"
}

append config {
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="VM"/>}

append_if [expr $use_cpu_load] config {
		<service name="TRACE"/>}

append config {
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>

	<start name="platform_reports">
		<binary name="report_rom"/>
		<resource name="RAM" quantum="1M"/>
		<provides>
			<service name="Report"/>
			<service name="ROM"/>
		</provides>
		<config>
			<policy label="pci_decode -> system" report="acpi -> acpi"/>
			<policy label="platform -> devices"  report="pci_decode -> devices"/>
		</config>
	</start>

	<start name="acpi" caps="350">
		<resource name="RAM" quantum="4M"/>
		<route>
			<service name="Report"> <child name="platform_reports"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="pci_decode" caps="350">
		<resource name="RAM" quantum="2M"/>
		<route>
			<service name="ROM" label="system"> <child name="platform_reports"/> </service>
			<service name="Report"> <child name="platform_reports"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="platform" caps="400" managing_system="yes">
		<resource name="RAM" quantum="4M"/>
		<provides>
			<service name="Platform"/>
		</provides>
		<config>
			<policy label_prefix="ps2">  <device name="ps2"/>    </policy>
			<policy label_prefix="ahci"> <pci class="AHCI"/>     </policy>
			<policy label_prefix="nic" info="yes"> <pci class="ETHERNET"/> </policy>
			<policy label_prefix="audio">
				<pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label_prefix="fb"  info="yes"> <pci class="VGA"/> </policy>
			<policy label_prefix="usb" info="yes"> <pci class="USB"/> </policy>
		</config>
		<route>
			<service name="ROM" label="devices"> <child name="platform_reports"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="ahci" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<config>
			<policy label_prefix="part_block" device="0" writeable="yes"/>
		</config>
	</start>

	<start name="ps2" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<config/>
		<route>
			<service name="Event"> <child name="event_filter" label="ps2"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="fb" priority="-1" caps="150">
		<binary name="vesa_fb"/>
		<resource name="RAM" quantum="32M"/>
		<config/>
	</start>

	<start name="pc_rtc" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides>
			<service name="Rtc"/>
		</provides>
	</start>

	<start name="part_block" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<route>
			<any-service><child name="ahci"/> <parent/><any-child/></any-service>
		</route>
		<config>}
append_if [expr $use_rumpfs] config {
			<policy label_prefix="rump_fs" partition="4" writeable="yes"/>}
append_if [expr !$use_rumpfs] config {
			<policy label_prefix="vbox1 -> raw" partition="3" writeable="yes"/>}
append config {
		</config>
	</start>}

append_if [have_spec nova] config {
	<start name="log_kernel">
		<binary name="log_core"/>
		<resource name="RAM" quantum="10M"/>
		<config period_ms="2000"/>
		<route>
			<service name="ROM" unscoped_label="log_core"> <parent/> </service>
			<service name="ROM" unscoped_label="ld.lib.so"> <parent/> </service>
			<service name="ROM" label="log"> <parent label="kernel_log"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="PD"> <parent/> </service>
			<service name="CPU"> <parent/> </service>
			<service name="LOG"> <parent/> </service>
		</route>
	</start>}

append_if [expr $use_rumpfs] config {
	<start name="rump_fs" priority="-1" caps="200">
		<binary name="vfs"/>
		<resource name="RAM" quantum="64M"/>
		<provides><service name="File_system"/></provides>
		<config ld_verbose="yes">
			<vfs>
				<rump fs="ext2fs" ram="48M"/>
			</vfs>}

append_if [expr $use_rumpfs && $use_ram_fs && $use_overlay_from_disk] config {
			<policy label_prefix="overlay_from_disk" root="/ram" writeable="no"/>}
append_if [expr $use_rumpfs] config {
			<default-policy root="/" writeable="} $rump_writeable {"/>
		</config>
		<route>
			<any-service><child name="part_block"/> <parent/><any-child/></any-service>
		</route>
	</start>}

append_if [expr $use_ram_fs] config {
	<start name="ram_fs" priority="-1" caps="1800">
		<binary name="vfs"/>}
append_if [expr $use_ram_fs] config "
		<resource name=\"RAM\" quantum=\"[expr $use_vms * 8 * 128]M\"/>"
append_if [expr $use_ram_fs] config {
		<provides><service name="File_system"/></provides>
		<config>
			<vfs>
				<ram/>
				<import>}
for { set i 1} { $i <= ${use_vms} } { incr i} {
append_if [expr $use_ram_fs] config "
					<dir name=\"ram${i}\">
						<rom name=\"${overlay_image}\"/>
					</dir>"
}
append_if [expr $use_ram_fs] config {
				</import>
			</vfs>

			<!-- constrain sessions according to their labels -->}
for { set i 1} { $i <= ${use_vms} } { incr i} {
append_if [expr $use_ram_fs] config "
			<policy label_prefix=\"vbox${i} -> from_ram_fs\" root=\"/ram${i}\" writeable=\"yes\"/>"
}
append_if [expr $use_ram_fs] config {
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
		<resource name="RAM" quantum="32M"/>
		<provides><service name="ROM"/></provides>
		<route>}
append_if [expr $use_rumpfs && $use_ram_fs && $use_overlay_from_disk] config {
			<service name="File_system"><child name="rump_fs"/></service>}
append_if [expr $use_ram_fs && $use_overlay_from_disk] config {
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>}

append_if [expr $use_cpu_load] config {
	<start name="trace_subject_reporter" priority="-1">
		<resource name="RAM" quantum="6M"/>
		<config period_ms="500">
			<report activity="yes" affinity="yes"/>
		</config>
		<route>
			<service name="TRACE"><parent label=""/></service>
			<service name="LOG"><parent/></service>
			<service name="Timer"><child name="timer"/></service>
			<service name="Report"><child name="report_rom"/></service>
			<any-service><parent/></any-service>
		</route>
	</start>

	<start name="cpu_load_display" priority="-1">
		<resource name="RAM" quantum="6M"/>
		<config/>
		<route>
			<service name="LOG"><parent/></service>
			<service name="Timer"><child name="timer"/></service>
			<service name="ROM" label="trace_subjects"><child name="report_rom"/></service>
			<service name="Gui"><child name="nitpicker"/></service>
			<any-service><parent/></any-service>
		</route>
	</start>

	<start name="top" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<config period_ms="10000"/>
		<route>
			<service name="TRACE"><parent label=""/></service>
			<service name="LOG"><parent/></service>
			<service name="Timer"><child name="timer"/></service>
			<service name="Report"><child name="report_rom"/></service>
			<any-service><parent/></any-service>
		</route>
	</start>}

append_if [expr $use_usb] config {
	<start name="usb" caps="200">
		<binary name="pc_usb_host"/>
		<resource name="RAM" quantum="20M"/>
		<provides> <service name="Usb"/> </provides>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>
			<any-service><parent/><any-child/></any-service>
		</route>
		<config>
			<report devices="yes"/>
			<policy label_prefix="usb_hid"><device class="0x3"/></policy>
			<policy label_prefix="vbox1">
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
			</policy>
		</config>
	</start>

	<start name="usb_hid" priority="-1" caps="140">
		<resource name="RAM" quantum="11M"/>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>
			<service name="Event"> <child name="event_filter" label="usb_hid"/> </service>
			<service name="ROM" label="report"> <child name="report_rom"/> </service>
			<service name="Usb"> <child name="usb"/> </service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>}

append_if [expr !$use_usb] config {
	<start name="usb" caps="200">
		<binary name="black_hole"/>
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Usb"/> </provides>
		<config> <usb/> </config>
	</start>
}

append_if [expr $use_serial] config {
	<start name="log_terminal" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Terminal"/>
		</provides>
	</start>}

append config $config_of_app

append config {
</config>
}

install_config $config

lappend boot_modules {*}[build_artifacts]

lappend_if [expr $use_ram_fs && !$use_overlay_from_disk] boot_modules $overlay_image
