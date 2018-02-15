set demo_component test/mesa_demo/gears
set demo_binary    gears
set demo_ram_quota 32M
set demo_caps      200

set demo_config {
<config ld_verbose="true">
	<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc" />
	<vfs>
		<dir name="dev">
			<log/> <inline name="rtc">2000-01-01 00:00</inline>
		</dir>
	</vfs>
</config>}

set demo_modules {
	gears
}

source ${genode_dir}/repos/libports/run/mesa.inc
