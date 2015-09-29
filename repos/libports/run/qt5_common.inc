source ${genode_dir}/repos/libports/run/qt5_drivers.inc

set feature(Input) 1
set feature(Framebuffer) 1
set feature(Timer) 1

#
# Build
#

proc qt5_build_components { feature_arg } {

	upvar $feature_arg feature

	set build_components {
		core
		init
	}

	append build_components [drivers_build_components feature]

	append build_components {
		server/nitpicker
		server/report_rom
		app/floating_window_layouter
		app/decorator
		app/pointer
		server/wm
	}

	return $build_components
}


#
# Configuration
#

proc qt5_parent_provides { feature_arg } {

	upvar $feature_arg feature

	set parent_provides [drivers_parent_provides feature]

	return $parent_provides
}

proc qt5_start_nodes { feature_arg } {

	upvar $feature_arg feature

	set start_nodes [drivers_start_nodes feature]

	append start_nodes {
		<start name="nitpicker">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Nitpicker"/></provides>
			<route> }
	append_if [use_fb_sdl feature] start_nodes { 
				<service name="Framebuffer"> <child name="fb_sdl" /> </service>
				<service name="Input"> <child name="fb_sdl" /> </service>
	}
	append_if [use_fb_drv feature] start_nodes {
				<service name="Framebuffer"> <child name="fb_drv" /> </service>
	}
	append_if [use_ps2_drv feature] start_nodes {
				<service name="Input"> <child name="ps2_drv" /> </service>
	}
	append start_nodes {
				<any-service> <parent /> <any-child /> </any-service>
			</route>
			<config>
				<domain name="pointer"   layer="1" xray="no" origin="pointer" />
				<domain name="decorator" layer="2" xray="opaque" />
				<domain name="default"   layer="2" />

				<policy label="pointer"         domain="pointer"/>
				<policy label=""                domain="default"/>
				<policy label="wm -> decorator" domain="decorator"/>

				<global-key name="KEY_SCROLLLOCK" operation="xray" />
				<global-key name="KEY_SYSRQ"      operation="kill" />
				<global-key name="KEY_PRINT"      operation="kill" />
				<global-key name="KEY_F11"        operation="kill" />
				<global-key name="KEY_F12"        operation="xray" />
			</config>
		</start>

		<start name="pointer">
			<resource name="RAM" quantum="1M"/>
			<route>
				<service name="Nitpicker"> <child name="nitpicker"/> </service>
				<any-service> <parent/> <any-child/> </any-service>
			</route>
		</start>

		<start name="report_rom">
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="Report"/>
				<service name="ROM"/>
			</provides>
			<config>
				<rom>
					<policy label="layouter -> window_list"     report="wm -> window_list"/>
					<policy label="layouter -> focus_request"   report="wm -> focus_request"/>
					<policy label="decorator -> window_layout"  report="layouter -> window_layout"/>
					<policy label="wm -> resize_request"        report="layouter -> resize_request"/>
					<policy label="decorator -> pointer"        report="wm -> pointer"/>
					<policy label="layouter -> hover"           report="decorator -> hover"/>
					<policy label="wm -> focus"                 report="layouter -> focus"/>
				</rom>
			</config>
		</start>

		<start name="wm">
			<resource name="RAM" quantum="8M"/>
			<provides>
				<service name="Nitpicker"/>
			</provides>
			<config>
				<policy label="decorator" role="decorator"/>
				<policy label="layouter"  role="layouter"/>
			</config>
			<route>
				<any-service>
					<child name="nitpicker"/> <child name="report_rom"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="layouter">
			<binary name="floating_window_layouter"/>
			<resource name="RAM" quantum="4M"/>
			<route>
				<any-service>
					<child name="wm"/> <child name="report_rom"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="decorator">
			<binary name="decorator"/>
			<resource name="RAM" quantum="8M"/>
			<route>
				<any-service>
					<child name="wm"/> <child name="report_rom"/> <parent/> <any-child/>
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

	set boot_modules {
		core
		init
	}

	append boot_modules [drivers_boot_modules feature]

	append boot_modules {
		nitpicker
		report_rom
		floating_window_layouter
		decorator
		wm
		pointer
	}

	return $boot_modules
}


#
# tar archive
#

proc create_qt5_fs_tar_archive { app_name qt_modules } {

	exec rm -rf bin/qt5_fs/${app_name}/qt

	if { [lsearch ${qt_modules} "gui"] != -1 } {
		# add fonts
		exec mkdir -p bin/qt5_fs/${app_name}/qt/lib
		exec ln -sf [pwd]/bin/qt5_fs/qt/lib/fonts bin/qt5_fs/${app_name}/qt/lib/fonts
	}

	if { [lsearch ${qt_modules} "quick"] != -1 } {
		# add QtQuick plugins
		exec mkdir -p bin/qt5_fs/${app_name}/qt
		exec ln -sf [pwd]/bin/qt5_fs/qt/qml bin/qt5_fs/${app_name}/qt/qml
	}

	exec tar chf bin/qt5_fs_${app_name}.tar -C bin/qt5_fs/${app_name} .
}
