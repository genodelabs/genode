# Tested for nova. 
assert_spec nova
 
if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit 0
}

set vdi_image "${flavor}.vdi"
set vbox_file "vm_${flavor}.vbox"
set overlay_image "overlay_${flavor}.vdi"

set build_components { }
set boot_modules { }

set config_of_app {
	<start name="vbox" priority="-2">
		<binary name="virtualbox" />
		<resource name="RAM" quantum="1280M"/>}
append config_of_app "
			<config vbox_file=\"${vbox_file}\" vm_name=\"${flavor}\">"
append config_of_app {
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc">
				<vfs>
					<dir name="dev"> <log/> <rtc/> </dir>}

append_if [expr $use_ram_fs] config_of_app {
					<dir name="ram">  <fs label="from_ram_fs"/> </dir>}

append config_of_app "
					<rom name=\"${vbox_file}\"/>"
append config_of_app {
					<fs />
				</vfs>
			</libc>
		</config>
		<route>}

append_if [expr $use_ram_fs] config_of_app {
			<service name="File_system">
				<if-arg key="label" value="from_ram_fs" />
				<child name="ram_fs"/>
			</service>}

append config_of_app {
			<service name="File_system"> <child name="rump_fs"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>
}

source ${genode_dir}/repos/ports/run/virtualbox_auto.inc

# copy vbox configuration to bin directory
exec cp ${genode_dir}/repos/ports/run/${vbox_file} bin/.

append boot_modules " ${vbox_file} "

build_boot_image $boot_modules

run_genode_until {\[init -\> vbox\].*Guest Additions capability report:.*seamless: no, hostWindowMapping: no, graphics: no} 70
run_genode_until {\[init -\> vbox\].*Guest Additions capability report:.*seamless: yes, hostWindowMapping: no, graphics: yes} 170 [output_spawn_id]

# cleanup bin directory - remove vbox file
exec rm bin/${vbox_file}
