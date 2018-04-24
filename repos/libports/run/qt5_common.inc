source ${genode_dir}/repos/libports/run/qt5_drivers.inc

set feature(Input) 1
set feature(Framebuffer) 1
set feature(Timer) 1

create_boot_directory

import_from_depot genodelabs/src/[base_src] \
                  genodelabs/src/decorator \
                  genodelabs/src/expat \
                  genodelabs/src/floating_window_layouter \
                  genodelabs/src/freetype \
                  genodelabs/src/init \
                  genodelabs/src/jpeg \
                  genodelabs/src/libc \
                  genodelabs/src/libpng \
                  genodelabs/src/mesa \
                  genodelabs/src/nitpicker \
                  genodelabs/src/pcre16 \
                  genodelabs/src/qt5_core \
                  genodelabs/src/qt5_gui \
                  genodelabs/src/qt5_qjpeg \
                  genodelabs/src/qt5_qpa_nitpicker \
                  genodelabs/src/report_rom \
                  genodelabs/src/stdcxx \
                  genodelabs/src/vfs \
                  genodelabs/src/wm \
                  genodelabs/src/zlib \
                  genodelabs/raw/qt5_dejavusans

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
				<service name="Report"> <child name="report_rom"/> </service>
				<any-service> <parent /> <any-child /> </any-service>
			</route>
			<config>
				<report focus="yes" hover="yes" xray="yes"/>

				<domain name="pointer" layer="1" label="no" content="client" origin="pointer" />
				<domain name="default" layer="2" label="no" content="client" focus="click" hover="always" />

				<policy label_prefix="pointer" domain="pointer"/>
				<default-policy domain="default"/>
			</config>
		</start>

		<start name="pointer">
			<resource name="RAM" quantum="1M"/>
			<provides> <service name="Report"/> </provides>
			<config shapes="yes"/>
			<route>
				<service name="Nitpicker"> <child name="nitpicker"/> </service>
				<service name="ROM" label="hover"> <child name="report_rom"/> </service>
				<service name="ROM" label="xray"> <child name="report_rom"/> </service>
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
				<policy label="layouter -> window_list"       report="wm -> window_list"/>
				<policy label="layouter -> focus_request"     report="wm -> focus_request"/>
				<policy label="decorator -> window_layout"    report="layouter -> window_layout"/>
				<policy label="wm -> resize_request"          report="layouter -> resize_request"/>
				<policy label="decorator -> pointer"          report="wm -> pointer"/>
				<policy label="layouter -> hover"             report="decorator -> hover"/>
				<policy label="wm -> focus"                   report="layouter -> focus"/>
				<policy label="clipboard -> focus"            report="nitpicker -> focus"/>
				<policy label="layouter -> decorator_margins" report="decorator -> decorator_margins"/>
				<policy label="pointer -> hover"              report="nitpicker -> hover"/>
				<policy label="pointer -> xray"               report="nitpicker -> xray"/>
			</config>
		</start>

		<start name="wm" caps="150">
			<resource name="RAM" quantum="12M"/>
			<provides>
				<service name="Nitpicker"/>
			</provides>
			<config>
				<policy label_prefix="decorator" role="decorator"/>
				<policy label_prefix="layouter"  role="layouter"/>
			</config>
			<route>
				<service name="ROM" label="focus">          <child name="report_rom"/> </service>
				<service name="ROM" label="resize_request"> <child name="report_rom"/> </service>
				<service name="Report">                     <child name="report_rom"/> </service>
				<any-service>
					<child name="nitpicker"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="layouter">
			<binary name="floating_window_layouter"/>
			<resource name="RAM" quantum="4M"/>
			<route>
				<service name="ROM" label="window_list">       <child name="report_rom"/> </service>
				<service name="ROM" label="focus_request">     <child name="report_rom"/> </service>
				<service name="ROM" label="hover">             <child name="report_rom"/> </service>
				<service name="ROM" label="decorator_margins"> <child name="report_rom"/> </service>
				<service name="Report">                        <child name="report_rom"/> </service>
				<any-service>
					<child name="wm"/> <parent/> <any-child/>
				</any-service>
			</route>
		</start>

		<start name="decorator">
			<binary name="decorator"/>
			<resource name="RAM" quantum="12M"/>
			<route>
				<service name="ROM" label="window_layout"> <child name="report_rom"/> </service>
				<service name="ROM" label="pointer">       <child name="report_rom"/> </service>
				<service name="Report">                    <child name="report_rom"/> </service>
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
