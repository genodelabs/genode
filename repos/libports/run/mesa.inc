
if {[have_spec linux] && $use_i965} {
	puts "i965 driver not supported on Linux."
	exit 1
}

if {[have_include power_on/qemu] && $use_i965} {
	puts "i965 driver not supported in Qemu."
	exit 1
}

create_boot_directory
import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/pkg/[drivers_interactive_pkg] \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/demo \
                  [depot_user]/src/init


set build_components {
	drivers/gpu/intel
	lib/mesa/swrast
}

lappend_if $use_i965 build_components lib/mesa/i965
lappend build_components $demo_component 
build $build_components

set config {
<config prio_levels="2">
	<parent-provides>
		<service name="CPU"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="ROM"/>
	</parent-provides>
	<default caps="100"/>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>

	<start name="drivers" caps="1000">
		<resource name="RAM" quantum="32M" constrain_phys="yes"/>
		<binary name="init"/>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<provides>
			<service name="Input"/> <service name="Framebuffer"/>
		</provides>
	</start>

	<start name="nitpicker" caps="200">
		<resource name="RAM" quantum="2M"/>
		<provides><service name="Nitpicker"/></provides>
		<config>
			<domain name="pointer" layer="1" content="client" label="no" origin="pointer" />
			<domain name="default" layer="2" content="client" label="no" focus="click" hover="always" />
			<policy label_prefix="pointer" domain="pointer"/>
			<default-policy                domain="default"/>
		</config>
		<route><any-service><parent/><any-child/></any-service></route>
	</start>
	<start name="pointer">
		<resource name="RAM" quantum="1M"/>
	</start>}

set backend_library egl_swrast.lib.so
if {$use_i965} {
set backend_library egl_i965.lib.so
}

set launchpad_config {
<config>
	<launcher name="init" ram_quota="1G" caps="2000">
		<config verbose="yes">
			<parent-provides>
				<service name="ROM"/>
				<service name="IRQ"/>
				<service name="IO_MEM"/>
				<service name="IO_PORT"/>
				<service name="PD"/>
				<service name="RM"/>
				<service name="CPU"/>
				<service name="LOG"/>
				<service name="Nitpicker"/>
				<service name="Timer"/>
				<service name="Gpu"/>
			</parent-provides>

			<start name="liquid_fb" caps="200">
				<resource name="RAM" quantum="16M"/>
				<provides> <service name="Framebuffer"/> <service name="Input"/> </provides>
				<config resize_handle="off" animate="off" title="gears" width="600" height="600" xpos="100" ypos="100"/>
				<route>
				<any-service> <parent/> <any-child/> </any-service>
				</route>
			</start>}

append launchpad_config "
			<start name=\"$demo_binary\" caps=\"$demo_caps\">
				<resource name=\"RAM\" quantum=\"$demo_ram_quota\"/>"

append launchpad_config $demo_config

append launchpad_config "
				<route>
					<service name=\"ROM\" label=\"egl_drv.lib.so\">
						<parent label=\"$backend_library\"/>
					</service>"
append launchpad_config {
					<any-service> <parent/> <any-child/> </any-service>
				</route>
			</start>
			</config>
	</launcher>
</config>}

append config {
	<start name="launchpad" caps="512">
		<resource name="RAM" quantum="512M"/>}
append config $launchpad_config
append config {
	</start>}

append_if $use_i965 config {
	<start name="intel_fb_drv" caps="200">
		<binary name="intel_gpu_drv"/>
		<resource name="RAM" quantum="40M"/>
		<provides> <service name="Gpu"/> </provides>
		<route>
			<any-service> <parent /> <any-child/> </any-service>
		</route>
	</start>}

append config {
</config>}

install_config $config

set boot_modules {
	libc.lib.so vfs.lib.so libm.lib.so
	egl.lib.so mesa.lib.so stdcxx.lib.so
	expat.lib.so glapi.lib.so

	egl_swrast.lib.so
}

append_if $use_i965 boot_modules {
	egl_i965.lib.so drm.lib.so
	intel_gpu_drv
}

append boot_modules $demo_modules
build_boot_image $boot_modules

append qemu_args " -m 768"

run_genode_until forever
