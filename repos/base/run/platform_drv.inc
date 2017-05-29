proc have_platform_drv {} {
	if {[have_spec linux]} {
		return 0
	}

	return [expr [have_spec arndale] \
	          || [have_spec imx53] \
	          || [have_spec rpi] \
	          || [have_spec odroid_x2] \
	          || [have_spec x86]]
}


##
# Return 1 if the scenario depends on the USB driver on the used platform
#
# On most ARM platform, user input is provided by an USB HID device. On other
# platforms, the USB driver may be omitted.
#
proc need_usb_hid { } {

	# covered by fb_sdl
	if {[have_spec linux]} { return 0 }

	# covered by ps2_drv
	if {[have_spec x86]}   { return 0 }
	if {[have_spec pbxa9]} { return 0 }

	return [have_spec usb]
}


##
# Return name of the NIC driver binary
#
proc nic_drv_binary { } {
	if {[have_spec linux]} { return linux_nic_drv }
	if {[have_spec omap4] || [have_spec arndale] || [have_spec rpi]} { return usb_drv }
	if {!([have_spec imx53] || [have_spec riscv] || [have_spec odroid_xu] || [have_spec wand_quad])} { return nic_drv }
}


##
# Return name of the audio driver binary
#
proc audio_drv_binary { } {
	if {[have_spec linux]} { return linux_audio_drv }
	if {[have_spec x86]}   { return audio_drv }
}


proc platform_drv_build_components {} {
	set drv_build_components ""
	lappend_if [have_platform_drv] drv_build_components drivers/platform
	lappend_if [have_spec acpi]    drv_build_components drivers/acpi
	lappend_if [have_spec acpi]    drv_build_components server/report_rom
	lappend_if [have_spec nova]    drv_build_components drivers/platform/spec/x86/device_pd
	return $drv_build_components
}


proc append_platform_drv_build_components {} {
	global build_components
	append build_components { } [platform_drv_build_components]
}

proc platform_drv_boot_modules {} {
	set drv_boot_modules ""
	lappend_if [have_platform_drv] drv_boot_modules platform_drv
	lappend_if [have_spec acpi]    drv_boot_modules acpi_drv
	lappend_if [have_spec acpi]    drv_boot_modules report_rom
	lappend_if [have_spec nova]    drv_boot_modules device_pd
	lappend_if [have_spec muen]    drv_boot_modules acpi
	return $drv_boot_modules
}

proc append_platform_drv_boot_modules {} {
	global boot_modules
	append boot_modules { } [platform_drv_boot_modules]
}


proc platform_drv_policy {} {

	if ([have_spec x86]) {
		return {
			<policy label_prefix="ps2_drv">  <device name="PS2"/> </policy>
			<policy label_prefix="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label_prefix="fb_drv">   <pci class="VGA"/> </policy>
			<policy label_prefix="wifi_drv"> <pci class="WIFI"/> </policy>
			<policy label_prefix="usb_drv">  <pci class="USB"/> </policy>
			<policy label_prefix="ahci_drv"> <pci class="AHCI"/> </policy>
			<policy label_prefix="audio_drv"> <pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label_prefix="intel_fb_drv" irq_mode="nomsi">
				<pci class="VGA"/>
				<pci bus="0" device="0" function="0"/>
				<pci class="ISABRIDGE"/>
			</policy>}
	} else {
		return {}
	}
}


proc platform_drv_priority {} { return "" }
proc platform_drv_add_routing {} {
	if {[have_spec acpi]} {
		return {
			<service name="ROM" label="system"> <child name="acpi_report_rom"/> </service>}
	}

	return ""
}


proc platform_drv_config_config {} {
	if {[have_spec acpi] || [have_spec arm] || [have_spec muen]} {
		return {
		<config>}
	}

	return {
		<config acpi="no">}
}


proc platform_drv_config {} {
	set drv_config ""

	if {[have_spec acpi]} {

		append drv_config {
	<start name="acpi_drv" } [platform_drv_priority] { caps="200" >
		<resource name="RAM" quantum="3M"/>
		<route>
			<service name="IO_MEM"> <parent/> </service>
			<service name="LOG">    <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="RM">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
			<service name="Report"> <child name="acpi_report_rom"/> </service>
		</route>
	</start>}

		append drv_config "
	<start name=\"acpi_report_rom\" [platform_drv_priority]>"

		append drv_config {
		<binary name="report_rom"/>
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="ROM" />
			<service name="Report" />
		</provides>
		<config>
			<policy label="platform_drv -> acpi" report="acpi_drv -> acpi"/>
		</config>
		<route>
			<service name="LOG"> <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>
		</route>
	</start>}

	}

	if {[have_platform_drv]} {

		append drv_config {
	<start name="platform_drv" } [platform_drv_priority] { caps="800">
		<resource name="RAM" quantum="4M" constrain_phys="yes"/>
		<provides>
			<service name="Platform"/>}

		append_if [have_spec acpi] drv_config {
			<service name="Acpi"/>}

		append_if [have_spec arm] drv_config {
			<service name="Regulator"/>}

		append drv_config {
		</provides>
		<route>}

		append drv_config "[platform_drv_add_routing]"

		append_if [have_spec acpi] drv_config {
			<service name="ROM" label="acpi"> <child name="acpi_report_rom"/> </service>}

		append_if [have_spec rpi] drv_config {
			<service name="Timer"> <any-child/> </service>}

		append drv_config {
			<any-service> <parent/> </any-service>
		</route>}

		append drv_config [platform_drv_config_config]
		append drv_config [platform_drv_policy]

		append drv_config {
		</config>
	</start>}

	}

	return $drv_config
}


proc append_platform_drv_config {} {
	global config
	append config [platform_drv_config]
	return $config
}
