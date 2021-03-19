set demo_component test/mesa_demo/triangle_gl
set demo_binary    triangle_gl
set demo_ram_quota 34M
set demo_caps      200

set demo_config {
<config ld_verbose="true">
	<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc"/>
	<vfs>
		<dir name="dev">
			<log/> <inline name="rtc">2020-01-01 00:00</inline>
		</dir>
	</vfs>
	<arg value="triangle_gl"/>
	<env key="MESA_LOADER_DRIVER_OVERRIDE" value="iris"/>
	<env key="INTEL_DEBUG" value="buf, submit, pc, sync"/>
</config>}

set demo_modules {
	triangle_gl
}

source ${genode_dir}/repos/libports/run/mesa.inc
