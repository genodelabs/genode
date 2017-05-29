#
# Global variables required by platform_drv-related functions
#

set build_components { }
set config { }
set boot_modules { }

source ${genode_dir}/repos/base/run/platform_drv.inc

#
# Helper functions
#

proc use_audio_drv { feature_arg } {
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

proc use_input_filter { feature_arg } {
	upvar $feature_arg feature
	return [info exists feature(Input)]
}

proc use_nic_drv { feature_arg } {
	upvar $feature_arg feature
	return [expr {[info exists feature(Nic)] &&
	              ([have_spec cadence_gem] ||
	               [have_spec lan9118] ||
	               [have_spec linux] ||
	               [have_spec x86])}]
}

proc use_ps2_drv { feature_arg } {
	upvar $feature_arg feature
	return [have_spec ps2]
}

proc use_timer { feature_arg } {
	upvar $feature_arg feature
	return [info exists feature(Timer)]
}

proc use_usb_input { feature_arg } {
	upvar $feature_arg feature
	return [expr {[info exists feature(Input)] && 
	              ([need_usb_hid] ||
	               ([have_spec x86] && ![have_spec linux]))}]
}

proc use_usb_nic { feature_arg } {
	upvar $feature_arg feature
	return [expr {[info exists feature(Nic)] &&
	              ([have_spec omap4] ||
	               [have_spec arndale] ||
	               [have_spec rpi])}]
}

proc use_usb_drv { feature_arg } {
	upvar $feature_arg feature
	return [expr {[use_usb_input feature] || [use_usb_nic feature]}]
}

#
# Keyboard layout - this function can be overridden in a run script
# 
proc language_chargen { } { return "en_us" }

#
# Build
#

proc drivers_build_components { feature_arg } {

	upvar $feature_arg feature

	set build_components { }

	# This function appends to the global 'build_components' variable, not to
	# the local version defined above.
	append_platform_drv_build_components

	lappend_if [use_audio_drv feature]    build_components drivers/audio
	lappend_if [use_fb_drv feature]       build_components drivers/framebuffer
	lappend_if [use_fb_sdl feature]       build_components drivers/framebuffer/spec/sdl
	lappend_if [use_gpio_drv feature]     build_components drivers/gpio
	lappend_if [use_input_filter feature] build_components server/input_filter
	lappend_if [use_nic_drv feature]      build_components drivers/nic
	lappend_if [use_ps2_drv feature]      build_components drivers/input/spec/ps2
	lappend_if [use_timer feature]        build_components drivers/timer
	lappend_if [use_usb_drv feature]      build_components drivers/usb

	return $build_components
}

proc gpio_drv { } { if {[have_spec rpi] && [have_spec hw]}  { return hw_gpio_drv }
                    if {[have_spec rpi] && [have_spec foc]} { return foc_gpio_drv }
                    return gpio_drv }

#
# Configuration
#

proc drivers_parent_provides { feature_arg } {

	upvar $feature_arg feature

	set parent_provides { }

	# TODO: make dependent on features

	append parent_provides {
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	}

	return $parent_provides
}

proc drivers_start_nodes { feature_arg } {

	upvar $feature_arg feature

	set start_nodes { }

	# This function appends to the global 'config' variable, not to the
	# 'start_nodes' variable defined above.
	append_platform_drv_config

	append_if [use_audio_drv feature] start_nodes {
		<start name="audio_drv">
			<binary name="} [audio_drv_binary] {"/>
			<resource name="RAM" quantum="8M"/>
			<provides><service name="Audio_out"/></provides>
			<config/>
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

	append_if [use_gpio_drv feature] start_nodes "
		<start name=\"[gpio_drv]\">
			<resource name=\"RAM\" quantum=\"4M\"/>
			<provides><service name=\"Gpio\"/></provides>
			<config/>
		</start>"

	if { [use_input_filter feature] } {

		exec cp -f [genode_dir]/repos/os/src/server/input_filter/[language_chargen].chargen bin/

		append start_nodes {
			<start name="input_filter">
				<resource name="RAM" quantum="1M" />
				<provides> <service name="Input" /> </provides>
				<config>}
		append_if [use_ps2_drv feature] start_nodes {
					<input label="ps2"/>}
		append_if [use_usb_drv feature] start_nodes {
					<input label="usb"/>}
		append_if [use_fb_sdl feature] start_nodes {
					<input label="sdl"/>}
		append start_nodes {
					<output>
						<chargen>
							<merge>}
		append_if [use_ps2_drv feature] start_nodes {
								<input name="ps2"/>}
		append_if [use_usb_drv feature] start_nodes {
								<input name="usb"/>}
		append_if [use_fb_sdl feature] start_nodes {
								<input name="sdl"/>}
		append start_nodes {
							</merge>
							<mod1>
								<key name="KEY_LEFTSHIFT"/> <key name="KEY_RIGHTSHIFT"/>
							</mod1>
							<mod2>
								<key name="KEY_LEFTCTRL"/> <key name="KEY_RIGHTCTRL"/>
							</mod2>
							<mod3>
								<key name="KEY_RIGHTALT"/> <!-- AltGr -->
							</mod3>
							<repeat delay_ms="500" rate_ms="50"/>}
		append start_nodes "
							<include rom=\"[language_chargen].chargen\"/>"
		append start_nodes {
						</chargen>
					</output>
				</config>
				<route>
					<service name="LOG"> <parent/> </service>
					<service name="CPU"> <parent/> </service>
					<service name="ROM"> <parent/> </service>
					<service name="PD">  <parent/> </service>
					<service name="Timer"> <child name="timer"/> </service>}
		append_if [use_ps2_drv feature] start_nodes {
					<service name="Input" label="ps2"> <child name="ps2_drv" /> </service>}
		append_if [use_usb_drv feature] start_nodes {
					<service name="Input" label="usb"> <child name="usb_drv" /> </service>}
		append_if [use_fb_sdl feature] start_nodes {
					<service name="Input" label="sdl"> <child name="fb_sdl" /> </service>}
		append start_nodes {
				</route>
			</start>
		}
	}

	append_if [use_nic_drv feature] start_nodes {
		<start name="nic_drv">
			<binary name="} [nic_drv_binary] {"/>
			<resource name="RAM" quantum="8M"/>
			<provides><service name="Nic"/></provides>
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
		append_if [use_usb_input feature] start_nodes {
					<service name="Input"/>}
		append_if [use_usb_nic feature] start_nodes {
					<service name="Nic"/>}
		append start_nodes {
				</provides>
				<config uhci="yes" ehci="yes" xhci="no">}
		append_if [use_usb_input feature] start_nodes {
					<hid/>}
		append_if [use_usb_nic feature] start_nodes {
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

	# This function appends to the global 'boot_modules' variable, not to the
	# local version defined above.
	append_platform_drv_boot_modules

	lappend_if [use_audio_drv feature]    boot_modules [audio_drv_binary]
	lappend_if [use_fb_drv feature]       boot_modules fb_drv
	lappend_if [use_fb_sdl feature]       boot_modules fb_sdl
	lappend_if [use_gpio_drv feature]     boot_modules [gpio_drv]
	lappend_if [use_input_filter feature] boot_modules input_filter
	lappend_if [use_input_filter feature] boot_modules [language_chargen].chargen
	lappend_if [use_nic_drv feature]      boot_modules [nic_drv_binary]
	lappend_if [use_ps2_drv feature]      boot_modules ps2_drv
	lappend_if [use_timer feature]        boot_modules timer
	lappend_if [use_usb_drv feature]      boot_modules usb_drv

	return $boot_modules
}

