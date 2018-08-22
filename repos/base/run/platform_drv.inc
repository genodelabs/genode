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
# Return name of the USB driver binary
#
proc usb_host_drv_binary { } {
	if {[have_spec arndale]}   { return arndale_usb_host_drv   }
	if {[have_spec panda]}     { return panda_usb_host_drv     }
	if {[have_spec rpi]}       { return rpi_usb_host_drv       }
	if {[have_spec wand_quad]} { return wand_quad_usb_host_drv }
	if {[have_spec odroid_x2]} { return odroid_x2_usb_host_drv }
	if {[have_spec x86]}       { return x86_pc_usb_host_drv    }
	return no_usb_drv_available
}


##
# Return name of the NIC driver binary
#
proc nic_drv_binary { } {
	if {[have_spec linux]} { return linux_nic_drv }
	if {[have_spec omap4] || [have_spec arndale] || [have_spec rpi]} { return usb_drv }
	if {!([have_spec riscv] || [have_spec odroid_xu])} { return nic_drv }
	return no_nic_drv_available
}


##
# Return configuration for NIC driver
#
proc nic_drv_config { } {
	if {[have_spec imx53]} {
		return "<config><card type=\"fsl,imx25-fec\" mii=\"rmii\" irq=\"87\" mmio=\"0x63fec000\"/></config>" }
	if {[have_spec wand_quad]} {
		return "<config><card type=\"fsl,imx6q-fec\" mii=\"rgmii\" irq=\"150\" mmio=\"0x2188000\"/></config>" }
	if {[have_spec omap4] || [have_spec arndale] || [have_spec rpi]} {
		return {<config uhci="yes" ohci="yes" ehci="yes"><nic mac="02:00:00:00:01:01"/></config>} }
	return ""
}


##
# Return name of the audio driver binary
#
proc audio_drv_binary { } {
	if {[have_spec linux]} { return linux_audio_drv }
	if {[have_spec x86]}   { return audio_drv }
	return no_audio_drv_available
}

proc acpi_drv_name { } {
	global use_acpica_as_acpi_drv
	if {[info exists use_acpica_as_acpi_drv] && $use_acpica_as_acpi_drv} {
		return acpica }

	return acpi_drv
}

proc platform_drv_build_components {} {
	set drv_build_components ""
	lappend_if [have_platform_drv] drv_build_components drivers/platform
	lappend_if [have_spec x86]     drv_build_components server/report_rom
	if {[have_spec x86]} {
		if {[acpi_drv_name] eq "acpi_drv"} {
			lappend drv_build_components drivers/acpi
		}
		if {[acpi_drv_name] eq "acpica"} {
			lappend drv_build_components app/acpica
		}
	}
	return $drv_build_components
}


proc append_platform_drv_build_components {} {
	global build_components
	append build_components { } [platform_drv_build_components]
}

proc platform_drv_boot_modules {} {
	set drv_boot_modules ""
	lappend_if [have_platform_drv] drv_boot_modules platform_drv
	lappend_if [have_spec x86]     drv_boot_modules report_rom

	if {[have_spec x86]} {
		if {[have_spec muen]} {
			lappend drv_boot_modules acpi
		} else {
			lappend drv_boot_modules [acpi_drv_name]
		}
	}

	return $drv_boot_modules
}

proc append_platform_drv_boot_modules {} {
	global boot_modules
	append boot_modules { } [platform_drv_boot_modules]
}


proc platform_drv_policy {} {

	if (![have_spec x86]) {
		return {}
	}

	set drv_policy ""

	if {[acpi_drv_name] eq "acpica"} {
		append drv_policy {
			<policy label="acpi_drv -> ">    <pci class="ALL"/> </policy>}
	}

	append drv_policy {
			<policy label_prefix="ps2_drv">  <device name="PS2"/> </policy>
			<policy label_prefix="nic_drv">  <pci class="ETHERNET"/> </policy>
			<policy label_prefix="fb_drv">   <pci class="VGA"/> </policy>
			<policy label_prefix="wifi_drv"> <pci class="WIFI"/> </policy>
			<policy label_prefix="usb_drv">  <pci class="USB"/> </policy>
			<policy label_prefix="ahci_drv"> <pci class="AHCI"/> </policy>
			<policy label_prefix="nvme_drv"> <pci class="NVME"/> </policy>
			<policy label_prefix="audio_drv"> <pci class="AUDIO"/> <pci class="HDAUDIO"/> </policy>
			<policy label_prefix="intel_fb_drv">
				<pci class="VGA"/>
				<pci bus="0" device="0" function="0"/>
				<pci class="ISABRIDGE"/>
			</policy>}

	return $drv_policy
}


proc platform_drv_priority {} { return "" }
proc platform_drv_add_routing {} {

	if {[acpi_drv_name] eq "acpica"} {
		return {
			<service name="ROM" label="acpi_ready"> <child name="acpi_report_rom"/> </service>}
	}

	return ""
}


proc platform_drv_config_config {} {
	if {[acpi_drv_name] eq "acpica"} {
		return {
		<config acpi_ready="yes">}
	}

	if {[have_spec acpi] || [have_spec arm] || [have_spec muen]} {
		return {
		<config>}
	}

	return {
		<config acpi="no">}
}


proc platform_drv_config {} {
	set drv_config ""

	if {[have_spec x86] && ![have_spec muen] && ![have_spec linux]} {

		append drv_config {
	<start name="acpi_drv" } [platform_drv_priority] { caps="400" >
		<binary name="} [acpi_drv_name] {"/>}

		if {[acpi_drv_name] eq "acpica"} {
			append drv_config {
		<resource name="RAM" quantum="5M"/>
		<config acpi_ready="yes" act_as_acpi_drv="yes" report="yes"/>}
		} else {
			append drv_config {
		<resource name="RAM" quantum="3M"/>}
		}

		append drv_config {
		<route>
			<service name="IO_MEM"> <parent/> </service>
			<service name="LOG">    <parent/> </service>
			<service name="PD">     <parent/> </service>
			<service name="RM">     <parent/> </service>
			<service name="CPU">    <parent/> </service>
			<service name="ROM">    <parent/> </service>}

		append_if [expr {[acpi_drv_name] eq "acpica"}] drv_config {
			<service name="IO_PORT">  <parent/> </service>
			<service name="IRQ">      <parent/> </service>
			<service name="Timer">    <child name="timer"/> </service>
			<service name="Platform"> <child name="platform_drv"/> </service>
			<service name="Acpi">     <child name="platform_drv"/> </service>}

		append drv_config {
			<service name="Report"> <child name="acpi_report_rom"/> </service>
			<service name="ROM" label="platform_info"> <parent/> </service>
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
			<policy label="platform_drv -> acpi" report="acpi_drv -> acpi"/>}

		append_if [expr {[acpi_drv_name] eq "acpica"}] drv_config {
			<policy label="platform_drv -> acpi_ready" report="acpi_drv -> acpi_ready"/>}

		append drv_config {
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

		append_if [have_spec x86] drv_config {
			<service name="Acpi"/>}

		append_if [have_spec arm] drv_config {
			<service name="Regulator"/>}

		append drv_config {
		</provides>
		<route>}

		append drv_config "[platform_drv_add_routing]"

		append_if [expr [have_spec x86] && ![have_spec muen]] drv_config {
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
