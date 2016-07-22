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

proc append_platform_drv_build_components {} {
	global build_components

	lappend_if [have_platform_drv] build_components drivers/platform
	lappend_if [have_spec acpi]    build_components drivers/acpi
	lappend_if [have_spec acpi]    build_components server/report_rom
	lappend_if [have_spec nova]    build_components drivers/platform/spec/x86/device_pd
}

proc append_platform_drv_boot_modules {} {
	global boot_modules

	lappend_if [have_platform_drv]        boot_modules platform_drv
	lappend_if [have_spec acpi]           boot_modules acpi_drv
	lappend_if [have_spec acpi]           boot_modules report_rom
	lappend_if [have_spec nova]           boot_modules device_pd
	lappend_if [have_spec hw_x86_64_muen] boot_modules acpi
}

proc platform_drv_policy {} {

	if ([have_spec x86]) {
		return {
			<policy label="ps2_drv">  <device name="PS2"/> </policy>
			<policy label="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label="fb_drv">   <pci class="VGA"/> </policy>
			<policy label="wifi_drv"> <pci class="WIFI"/> </policy>
			<policy label="usb_drv">  <pci class="USB"/> </policy>
			<policy label="ahci_drv"> <pci class="AHCI"/> </policy>
			<policy label="audio_drv"> <pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label="intel_fb_drv" irq_mode="nomsi">
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
	if {[have_spec acpi] || [have_spec arm] || [have_spec hw_x86_64_muen]} {
		return {
		<config>}
	}

	return {
		<config acpi="no">}
}

proc append_platform_drv_config {} {
	global config

	if {[have_spec acpi]} {

		append config "
	<start name=\"acpi_drv\" [platform_drv_priority]>"

		append config {
		<resource name="RAM" quantum="3M"/>
		<route>
			<service name="LOG"> <parent/> </service>
			<service name="IO_MEM"> <parent/> </service>
			<service name="RM"> <parent/> </service>
			<service name="Report"> <child name="acpi_report_rom"/> </service>
		</route>
	</start>}

		append config "
	<start name=\"acpi_report_rom\" [platform_drv_priority]>"

		append config {
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
			<service name="RM"> <parent/> </service>
		</route>
	</start>}

	}

	if {[have_platform_drv]} {

		append config "
	<start name=\"platform_drv\" [platform_drv_priority]>"

		append config {
		<resource name="RAM" quantum="4M" constrain_phys="yes"/>
		<provides>
			<service name="Platform"/>}

		append_if [have_spec acpi] config {
			<service name="Acpi"/>}

		append_if [have_spec arm] config {
			<service name="Regulator"/>}

		append config {
		</provides>
		<route>}

		append config "[platform_drv_add_routing]"

		append_if [have_spec acpi] config {
			<service name="ROM" label="acpi"> <child name="acpi_report_rom"/> </service>}

		append_if [have_spec rpi] config {
			<service name="Timer"> <any-child/> </service>}

		append config {
			<any-service> <parent/> </any-service>
		</route>}

		append config [platform_drv_config_config]
		append config [platform_drv_policy]

		append config {
		</config>
	</start>}

	}

	return $config
}
