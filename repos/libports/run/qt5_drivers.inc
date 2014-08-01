
#
# Helper functions
#

proc use_acpi_drv { feature_arg } {
	upvar $feature_arg feature
	return [have_spec acpi]
}

proc use_audio_out_drv { feature_arg } {
	upvar $feature_arg feature
	return [info exists feature(Audio_out)]
}

proc use_fb_drv { feature_arg } {
	upvar $feature_arg feature
	return [have_spec framebuffer]
}

proc use_fb_sdl { feature_arg } {
	upvar $feature_arg feature
	return [have_spec linux]
}

proc use_gpio_drv { feature_arg } {
	upvar $feature_arg feature
	return [expr {[use_usb_drv feature] &&
	              [have_spec gpio]}]
}

proc use_nic_drv { feature_arg } {
	upvar $feature_arg feature
	return [expr {[info exists feature(Nic)] &&
	              ![use_usb_drv feature]}]
}

proc use_pci_device_pd { feature_arg } {
	upvar $feature_arg feature
	return [expr [use_pci_drv feature] && [have_spec nova]]
}

proc use_pci_drv { feature_arg } {
	upvar $feature_arg feature
	return [have_spec pci]
}

proc use_ps2_drv { feature_arg } {
	upvar $feature_arg feature
	return [have_spec ps2]
}

proc use_timer { feature_arg } {
	upvar $feature_arg feature
	return [info exists feature(Timer)]
}

proc use_usb_drv { feature_arg } {
	upvar $feature_arg feature
	return [expr {([info exists feature(Input)] ||
	               [info exists feature(Nic)]) &&
                  (![use_ps2_drv feature] && [have_spec usb])}]
}

#
# Build
#

proc drivers_build_components { feature_arg } {

	upvar $feature_arg feature

	set build_components { }

	lappend_if [use_acpi_drv feature]      build_components drivers/acpi
	lappend_if [use_audio_out_drv feature] build_components drivers/audio_out
	lappend_if [use_fb_drv feature]        build_components drivers/framebuffer
	lappend_if [use_fb_sdl feature]        build_components drivers/framebuffer
	lappend_if [use_gpio_drv feature]      build_components drivers/gpio
	lappend_if [use_nic_drv feature]       build_components drivers/nic
	lappend_if [use_pci_drv feature]       build_components drivers/pci
	lappend_if [use_ps2_drv feature]       build_components drivers/input/ps2
	lappend_if [use_timer feature]         build_components drivers/timer
	lappend_if [use_usb_drv feature]       build_components drivers/usb

	return $build_components
}

#
# Configuration
#

proc drivers_parent_provides { feature_arg } {

	upvar $feature_arg feature

	set parent_provides { }

	# TODO: make dependent on features

	append parent_provides {
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
	}

	return $parent_provides
}

proc drivers_start_nodes { feature_arg } {

	upvar $feature_arg feature

	set start_nodes { }

	append_if [use_acpi_drv feature] start_nodes {
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
		</start>
	}

	append_if [use_audio_out_drv feature] start_nodes {
		<start name="audio_out_drv">
			<resource name="RAM" quantum="4M"/>
			<provides><service name="Audio_out"/></provides>
		</start>
	}

	append_if [use_fb_drv feature] start_nodes {
		<start name="fb_drv">
			<resource name="RAM" quantum="4M"/>
			<provides><service name="Framebuffer"/></provides>
		</start>
	}

	append_if [use_fb_sdl feature] start_nodes {
		<start name="fb_sdl">
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="Input"/>
				<service name="Framebuffer"/>
			</provides>
		</start>
	}

	append_if [use_gpio_drv feature] start_nodes {
		<start name="gpio_drv">
			<resource name="RAM" quantum="4M"/>
			<provides><service name="Gpio"/></provides>
			<config/>
		</start>
	}

	append_if [use_nic_drv feature] start_nodes {
		<start name="nic_drv">
			<resource name="RAM" quantum="2M"/>
			<provides><service name="Nic"/></provides>
		</start>
	}

	append_if [expr [use_pci_drv feature] && ![use_acpi_drv feature]] start_nodes {
		<start name="pci_drv">
			<resource name="RAM" quantum="2M"/>
			<provides><service name="PCI"/></provides>
		</start>
	}

	append_if [use_ps2_drv feature] start_nodes {
		<start name="ps2_drv">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Input"/></provides>
		</start>
	}

	append_if [use_timer feature] start_nodes {
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start>
	}

	if { [use_usb_drv feature] } {
		append start_nodes {
			<start name="usb_drv">
				<resource name="RAM" quantum="12M"/>
				<provides>}
		append_if [info exists feature(Input)] start_nodes {
					<service name="Input"/>}
		append_if [info exists feature(Nic)] start_nodes {
					<service name="Nic"/>}
		append start_nodes {
				</provides>
				<config uhci="yes" ehci="yes" xhci="no">}
		append_if [info exists feature(Input)] start_nodes {
					<hid/>}
		append_if [info exists feature(Nic)] start_nodes {
					<nic mac="2e:60:90:0c:4e:01" />}
		append start_nodes {
				</config>
			</start>
		}
	}

	return $start_nodes
}

#
# Boot modules
#

proc drivers_boot_modules { feature_arg } {

	upvar $feature_arg feature

	set boot_modules { }

	lappend_if [use_acpi_drv feature]      boot_modules acpi_drv
	lappend_if [use_audio_out_drv feature] boot_modules audio_out_drv
	lappend_if [use_fb_drv feature]        boot_modules fb_drv
	lappend_if [use_fb_sdl feature]        boot_modules fb_sdl
	lappend_if [use_gpio_drv feature]      boot_modules gpio_drv
	lappend_if [use_nic_drv feature]       boot_modules nic_drv
	lappend_if [use_pci_drv feature]       boot_modules pci_drv
	lappend_if [use_pci_device_pd feature] boot_modules pci_device_pd
	lappend_if [use_ps2_drv feature]       boot_modules ps2_drv
	lappend_if [use_timer feature]         boot_modules timer
	lappend_if [use_usb_drv feature]       boot_modules usb_drv

	return $boot_modules
}

