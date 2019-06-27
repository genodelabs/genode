
source ${genode_dir}/repos/libports/run/qt5_drivers.inc

set feature(Input) 1
set feature(Framebuffer) 1
set feature(Timer) 1

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/expat \
                  [depot_user]/src/freetype \
                  [depot_user]/src/init \
                  [depot_user]/src/jpeg \
                  [depot_user]/src/libc \
                  [depot_user]/src/libpng \
                  [depot_user]/src/mesa \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/global_keys_handler \
                  [depot_user]/src/rom_filter \
                  [depot_user]/src/pcre16 \
                  [depot_user]/src/qt5_core \
                  [depot_user]/src/qt5_gui \
                  [depot_user]/src/qt5_qjpeg \
                  [depot_user]/src/qt5_qpa_nitpicker \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/stdcxx \
                  [depot_user]/src/vfs \
                  [depot_user]/src/zlib \
                  [depot_user]/pkg/themed_wm \
                  [depot_user]/raw/qt5_dejavusans

#
# Build
#

proc qt5_build_components { feature_arg } {

	upvar $feature_arg feature

	set build_components { }

	append build_components [drivers_build_components feature]

	append build_components { }

	return $build_components
}


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

proc qt5_parent_provides { feature_arg } {

	upvar $feature_arg feature

	set parent_provides [drivers_parent_provides feature]

	return $parent_provides
}

proc qt5_start_nodes { feature_arg } {

	upvar $feature_arg feature

	set start_nodes [drivers_start_nodes feature]

	append start_nodes {
		<start name="nitpicker" caps="200">
			<resource name="RAM" quantum="2M"/>
			<provides><service name="Nitpicker"/></provides>
			<route> }
	append_if [use_fb_sdl feature] start_nodes { 
				<service name="Framebuffer"> <child name="fb_sdl" /> </service>
	}
	append_if [use_fb_drv feature] start_nodes {
				<service name="Framebuffer"> <child name="fb_drv" /> </service>
	}
	append start_nodes {
				<service name="Input">  <child name="input_filter"/> </service>
				<service name="Report"> <child name="wm_report_rom"/> </service>
				<any-service> <parent /> <any-child /> </any-service>
			</route>
			<config>
				<report focus="yes" hover="yes" xray="yes"/>

				<domain name="pointer" layer="1" label="no" content="client" origin="pointer" />
				<domain name="default" layer="2" label="no" content="client" focus="click" hover="always" />

				<policy label_prefix="pointer" domain="pointer"/>
				<default-policy domain="default"/>

				<global-key name="KEY_CAPSLOCK"  label="global_keys_handler -> input" />
				<global-key name="KEY_NUMLOCK"   label="global_keys_handler -> input" />
			</config>
		</start>

		<start name="pointer">
			<resource name="RAM" quantum="1M"/>
			<provides> <service name="Report"/> </provides>
			<config shapes="yes"/>
			<route>
				<service name="Nitpicker"> <child name="nitpicker"/> </service>
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
				<policy label="layouter -> focus_request"     report="wm -> focus_request"/>
				<policy label="decorator -> pointer"          report="wm -> pointer"/>
				<policy label="layouter -> window_list"       report="wm -> window_list"/>
				<policy label="wm -> focus"                   report="layouter -> focus"/>
				<policy label="wm -> resize_request"          report="layouter -> resize_request"/>
				<policy label="decorator -> window_layout"    report="layouter -> window_layout"/>
				<policy label="layouter -> rules"             report="layout_rules"/>
				<policy label="layouter -> decorator_margins" report="decorator -> decorator_margins"/>
				<policy label="layouter -> hover"             report="decorator -> hover"/>
				<policy label="clipboard -> focus"            report="nitpicker -> focus"/>
				<policy label="pointer -> hover"              report="nitpicker -> hover"/>
				<policy label="pointer -> xray"               report="nitpicker -> xray"/>
				<policy label="input_filter -> capslock"      report="global_keys_handler -> capslock"/>
				<policy label="numlock_remap_rom -> numlock"  report="global_keys_handler -> numlock"/>
				<policy label="ps2_drv -> capslock"           report="global_keys_handler -> capslock"/>
				<policy label="ps2_drv -> numlock"            report="global_keys_handler -> numlock"/>
				<policy label="usb_drv -> capslock"           report="global_keys_handler -> capslock"/>
				<policy label="usb_drv -> numlock"            report="global_keys_handler -> numlock"/>
			</config>
		</start>

		<start name="global_keys_handler">
			<resource name="RAM" quantum="1M"/>
			<config>
				<bool name="capslock" initial="no"/>
				<bool name="numlock"  initial="no"/>

				<press name="KEY_CAPSLOCK" bool="capslock" change="toggle"/>
				<press name="KEY_NUMLOCK"  bool="numlock"  change="toggle"/>

				<report name="capslock">     <bool name="capslock"/>     </report>
				<report name="numlock">      <bool name="numlock"/>      </report>
			</config>
			<route>
				<service name="Report">    <child name="wm_report_rom"/> </service>
				<service name="Nitpicker"> <child name="nitpicker"/>  </service>
				<service name="Timer">     <child name="timer"/>      </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
		</start>

		<start name="wm" caps="250">
			<resource name="RAM" quantum="8M"/>
			<provides>
				<service name="Nitpicker"/> <service name="Report"/> <service name="ROM"/>
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
			<resource name="RAM" quantum="16M"/>} [qt5_decorator_config] {
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

#
# Boot modules
#

proc qt5_boot_modules { feature_arg } {

	upvar $feature_arg feature

	set boot_modules { }

	append boot_modules [drivers_boot_modules feature]

	append boot_modules { }

	return $boot_modules
}
