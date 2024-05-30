create_boot_directory

import_from_depot [depot_user]/pkg/[drivers_interactive_pkg] \
                  [depot_user]/pkg/themed_wm \
                  [depot_user]/raw/qt5_dejavusans \
                  [depot_user]/src/[base_src] \
                  [depot_user]/src/expat \
                  [depot_user]/src/global_keys_handler \
                  [depot_user]/src/init \
                  [depot_user]/src/libdrm \
                  [depot_user]/src/libc \
                  [depot_user]/src/mesa \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/qt5_base \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/rom_filter \
                  [depot_user]/src/stdcxx \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_gpu \
                  [depot_user]/src/vfs_pipe \

#
# Configuration
#

proc qt5_layouter_config { } {
	return {
		<config rules="rom">
			<report rules="yes"/>
			<rules>
				<screen name="screen"/>
				<assign label_prefix="" target="screen" xpos="any" ypos="any"/>
			</rules>
		</config>}
}

proc qt5_decorator_binary { } { return "decorator" }

proc qt5_decorator_config { } {
	return {
		<config>
			<!-- this is an example config for the themed decorator
			<vfs>
				<tar name="plain_decorator_theme.tar"/>
			</vfs>
			<libc/>
			-->
			<default-policy/>
		</config>}
}

proc qt5_parent_provides { } {

	set parent_provides { }

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

#
# Keyboard layout - this function can be overridden in a run script
# 
proc language_chargen { } { return "en_us" }

exec mkdir -p bin
exec cp -f [genode_dir]/repos/os/src/server/event_filter/[language_chargen].chargen bin/
exec cp -f [genode_dir]/repos/os/src/server/event_filter/special.chargen bin/

set qt5_event_filter_config {
	<config>
		<output>
			<chargen>
				<remap>
					<include rom="numlock.remap"/>
					<merge>
						<input name="ps2"/>
						<input name="usb"/>
						<input name="sdl"/>
					</merge>
				</remap>
				<mod1>
					<key name="KEY_LEFTSHIFT"/> <key name="KEY_RIGHTSHIFT"/>
				</mod1>
				<mod2>
					<key name="KEY_LEFTCTRL"/> <key name="KEY_RIGHTCTRL"/>
				</mod2>
				<mod3>
					<key name="KEY_RIGHTALT"/> <!-- AltGr -->
				</mod3>
				<mod4>
					<rom name="capslock"/>
				</mod4>
				<repeat delay_ms="500" rate_ms="50"/>}
append qt5_event_filter_config "
				<include rom=\"[language_chargen].chargen\"/>"
append qt5_event_filter_config {
				<include rom="special.chargen"/>
			</chargen>
		</output>
		<policy label="ps2" input="ps2"/>
		<policy label="usb" input="usb"/>
		<policy label="sdl" input="sdl"/>
	</config>}

set fd [open bin/qt5_event_filter.config w]
puts $fd $qt5_event_filter_config
close $fd


proc qt5_start_nodes { } {

	set start_nodes { }

	append start_nodes {

		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start>

		<!-- toggle key mappings depending on the numlock state -->
		<start name="numlock_remap_rom">
			<binary name="rom_filter"/>
			<resource name="RAM" quantum="1M"/>
			<provides> <service name="ROM"/> </provides>
			<config>
				<input name="numlock_enabled" rom="numlock" node="numlock">
					<attribute name="enabled"/> </input>
				<output node="remap">
					<if>
						<has_value input="numlock_enabled" value="no"/>
						<then>
							<inline>
								<key name="KEY_KP0"   to="KEY_INSERT"/>
								<key name="KEY_KP1"   to="KEY_END"/>
								<key name="KEY_KP2"   to="KEY_DOWN"/>
								<key name="KEY_KP3"   to="KEY_PAGEDOWN"/>
								<key name="KEY_KP4"   to="KEY_LEFT"/>
								<key name="KEY_KP5"   to="KEY_RESERVED"/>
								<key name="KEY_KP6"   to="KEY_RIGHT"/>
								<key name="KEY_KP7"   to="KEY_HOME"/>
								<key name="KEY_KP8"   to="KEY_UP"/>
								<key name="KEY_KP9"   to="KEY_PAGEUP"/>
								<key name="KEY_KPDOT" to="KEY_DELETE"/>
							</inline>
						</then>
					</if>
				</output>
			</config>
			<route>
				<service name="ROM" label="numlock"> <child name="wm_report_rom"/> </service>
				<any-service> <parent/> </any-service>
			</route>
		</start>

		<start name="drivers" caps="1500" managing_system="yes">
			<resource name="RAM" quantum="64M"/>
			<binary name="init"/>
			<route>
				<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
				<service name="ROM" label="event_filter.config"> <parent label="qt5_event_filter.config"/> </service>
				<service name="ROM" label="event_filter -> capslock"> <child name="wm_report_rom"/> </service>
				<service name="ROM" label="event_filter -> numlock.remap"> <child name="numlock_remap_rom"/> </service>
				<service name="Timer">   <child name="timer"/> </service>
				<service name="Capture"> <child name="nitpicker"/> </service>
				<service name="Event">   <child name="nitpicker"/> </service>
				<any-service> <parent/> </any-service>
			</route>
		</start>

		<start name="nitpicker" caps="200">
			<resource name="RAM" quantum="2M"/>
			<provides>
				<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
			</provides>
			<route>
				<service name="Report"> <child name="wm_report_rom"/> </service>
				<any-service> <parent /> <any-child /> </any-service>
			</route>
			<config>
				<capture/> <event/>

				<report focus="yes" hover="yes" xray="yes"/>

				<domain name="pointer" layer="1" label="no" content="client" origin="pointer" />
				<domain name="default" layer="2" label="no" content="client"
				        focus="click" hover="always" width="1024" height="768"/>

				<policy label_prefix="pointer" domain="pointer"/>
				<default-policy domain="default"/>

				<global-key name="KEY_CAPSLOCK"  label="global_keys_handler -> input" />
				<global-key name="KEY_NUMLOCK"   label="global_keys_handler -> input" />
			</config>
		</start>

		<start name="pointer">
			<resource name="RAM" quantum="2M"/>
			<provides> <service name="Report"/> </provides>
			<config shapes="yes"/>
			<route>
				<service name="Gui"> <child name="nitpicker"/> </service>
				<service name="ROM" label="hover"> <child name="wm_report_rom"/> </service>
				<service name="ROM" label="xray"> <child name="wm_report_rom"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
		</start>

		<start name="wm_report_rom">
			<binary name="report_rom"/>
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="Report"/>
				<service name="ROM"/>
			</provides>
			<config verbose="no">
				<policy label="layouter -> focus_request"           report="wm -> focus_request"/>
				<policy label="decorator -> pointer"                report="wm -> pointer"/>
				<policy label="layouter -> window_list"             report="wm -> window_list"/>
				<policy label="wm -> focus"                         report="layouter -> focus"/>
				<policy label="wm -> resize_request"                report="layouter -> resize_request"/>
				<policy label="decorator -> window_layout"          report="layouter -> window_layout"/>
				<policy label="layouter -> rules"                   report="layout_rules"/>
				<policy label="layouter -> decorator_margins"       report="decorator -> decorator_margins"/>
				<policy label="layouter -> hover"                   report="decorator -> hover"/>
				<policy label="clipboard -> focus"                  report="nitpicker -> focus"/>
				<policy label="pointer -> hover"                    report="nitpicker -> hover"/>
				<policy label="pointer -> xray"                     report="nitpicker -> xray"/>
				<policy label="drivers -> event_filter -> capslock" report="global_keys_handler -> capslock"/>
				<policy label="numlock_remap_rom -> numlock"        report="global_keys_handler -> numlock"/>
				<policy label="ps2 -> capslock"                     report="global_keys_handler -> capslock"/>
				<policy label="ps2 -> numlock"                      report="global_keys_handler -> numlock"/>
				<policy label="usb -> capslock"                     report="global_keys_handler -> capslock"/>
				<policy label="usb -> numlock"                      report="global_keys_handler -> numlock"/>
			</config>
		</start>

		<start name="global_keys_handler">
			<resource name="RAM" quantum="1M"/>
			<config>
				<bool name="capslock" initial="no"/>
				<bool name="numlock"  initial="no"/>

				<press name="KEY_CAPSLOCK" bool="capslock" change="toggle"/>
				<press name="KEY_NUMLOCK"  bool="numlock"  change="toggle"/>

				<report name="capslock"> <bool name="capslock"/> </report>
				<report name="numlock">  <bool name="numlock"/>  </report>
			</config>
			<route>
				<service name="Report"> <child name="wm_report_rom"/> </service>
				<service name="Gui">    <child name="nitpicker"/>     </service>
				<service name="Timer">  <child name="timer"/>         </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
		</start>

		<start name="wm" caps="250">
			<resource name="RAM" quantum="8M"/>
			<provides>
				<service name="Gui"/> <service name="Report"/> <service name="ROM"/>
			</provides>
			<config>
				<policy label_prefix="decorator" role="decorator"/>
				<policy label_prefix="layouter"  role="layouter"/>
				<default-policy/>
			</config>
			<route>
				<service name="ROM" label="focus">          <child name="wm_report_rom"/> </service>
				<service name="ROM" label="resize_request"> <child name="wm_report_rom"/> </service>
				<service name="Report" label_last="shape">  <child name="pointer"/>       </service>
				<service name="Report">                     <child name="wm_report_rom"/> </service>
				<any-service>
					<child name="nitpicker"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="layouter">
			<binary name="window_layouter"/>
			<resource name="RAM" quantum="4M"/>} [qt5_layouter_config] {
			<route>
				<service name="ROM" label="window_list">       <child name="wm_report_rom"/> </service>
				<service name="ROM" label="focus_request">     <child name="wm_report_rom"/> </service>
				<service name="ROM" label="hover">             <child name="wm_report_rom"/> </service>
				<service name="ROM" label="decorator_margins"> <child name="wm_report_rom"/> </service>
				<service name="ROM" label="rules">             <child name="wm_report_rom"/> </service>
				<service name="Report" label="rules"> <child name="wm_report_rom" label="layout_rules"/> </service>
				<service name="Report">               <child name="wm_report_rom"/> </service>
				<any-service>
					<child name="wm"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="decorator" caps="300">
			<binary name="} [qt5_decorator_binary] {"/>
			<resource name="RAM" quantum="32M"/>} [qt5_decorator_config] {
			<route>
				<service name="ROM" label="window_layout"> <child name="wm_report_rom"/> </service>
				<service name="ROM" label="pointer">       <child name="wm_report_rom"/> </service>
				<service name="Report">                    <child name="wm_report_rom"/> </service>
				<any-service>
					<child name="wm"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>
	}

	return $start_nodes
}


proc qt5_boot_modules { } {

	set boot_modules [build_artifacts]

	lappend boot_modules [language_chargen].chargen
	lappend boot_modules special.chargen
	lappend boot_modules qt5_event_filter.config
}
