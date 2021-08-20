
if {[have_spec linux] && ($use_iris || $use_etnaviv)} {
	puts "Hardware driver not supported on Linux."
	exit 1
}

if {[have_include power_on/qemu] && ($use_iris || $use_etnaviv)} {
	puts "Hardware driver not supported in Qemu."
	exit 1
}

create_boot_directory

set imports { }

if {$use_etnaviv} {
lappend imports [depot_user]/pkg/drivers_managed-[board]
} else {
lappend imports [depot_user]/pkg/drivers_interactive-[board]
}

lappend imports [depot_user]/src/nitpicker
lappend imports [depot_user]/src/report_rom

import_from_depot $imports

set build_components {
	drivers/gpu/intel
	lib/mesa/softpipe
	core init timer
}

lappend_if $use_iris    build_components lib/mesa/iris
lappend_if $use_etnaviv build_components lib/mesa/etnaviv
lappend_if $use_etnaviv build_components drivers/gpu/etnaviv
lappend build_components $demo_component

build $build_components

set config {
<config prio_levels="2" verbose="yes">
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

	<start name="report_rom">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config verbose="no"/>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="drivers" caps="2500" managing_system="yes">
		<resource name="RAM" quantum="128M"/>
		<binary name="init"/>
		<provides> <service name="Platform"/> </provides>
		<route>
			<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
			<service name="Timer">   <child name="timer"/> </service>
			<service name="Capture"> <child name="nitpicker"/> </service>
			<service name="Event">   <child name="nitpicker"/> </service>
			<service name="Report">  <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="nitpicker" caps="200">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
		</provides>
		<config>
			<capture/> <event/>
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
if {$use_iris}    { set backend_library egl_iris.lib.so }
if {$use_etnaviv} { set backend_library egl_etnaviv.lib.so }

append config "
	<start name=\"$demo_binary\" caps=\"$demo_caps\">
		<resource name=\"RAM\" quantum=\"$demo_ram_quota\"/>"

append config $demo_config

append config "
		<route>
			<service name=\"ROM\" label=\"egl_drv.lib.so\">
				<parent label=\"$backend_library\"/>
			</service>"
append config {
			<service name="Gui"> <child name="nitpicker"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
}

append_if $use_iris config {
	<start name="intel_gpu_drv" caps="2000">
		<resource name="RAM" quantum="40M"/>
		<provides>
			<service name="Gpu"/>
			<service name="Platform"/>
		</provides>
		<route>
			<service name="Platform"> <child name="drivers"/> </service>
			<any-service> <parent /> <any-child/> </any-service>
		</route>
	</start>
}

append_if $use_etnaviv config {
	<start name="imx8q_gpu_drv" caps="200">
		<resource name="RAM" quantum="64M"/>
		<provides> <service name="Drm"/> </provides>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Platform"> <child name="drivers"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>}

append config {
</config>}

install_config $config

set boot_modules {
	libc.lib.so vfs.lib.so libm.lib.so
	egl.lib.so mesa.lib.so stdcxx.lib.so
	expat.lib.so glapi.lib.so zlib.lib.so
	core init ld.lib.so timer egl_swrast.lib.so
	libdrm.lib.so
}

append_if $use_iris boot_modules {
	egl_iris.lib.so libdrm.lib.so
	intel_gpu_drv
}

append_if $use_etnaviv boot_modules {
	egl_etnaviv.lib.so libdrm.lib.so

	imx8q_gpu_drv
}

append boot_modules $demo_modules
build_boot_image $boot_modules

append qemu_args " -m 768"

run_genode_until forever
