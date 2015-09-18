# Tested for nova. 
assert_spec nova
 
if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit 0
}

set vdi_image "${flavor}.vdi"
set vbox_file "vm_${flavor}.vbox"
set overlay_image "overlay_${flavor}.vdi"

set build_components {
	server/input_merger
	drivers/nic
	drivers/audio
	server/nitpicker
	app/vbox_pointer
	server/nit_fb
	server/report_rom
}

set boot_modules {
	input_merger
	nic_drv
	audio_drv
	nitpicker
	vbox_pointer
	nit_fb
	report_rom
}

set config_of_app {

	<start name="input_merger">
		<resource name="RAM" quantum="1M" />
		<provides>
			<service name="Input" />
		</provides>
		<config>}
append_if [expr $use_ps2] config_of_app {
			<input label="ps2" /> }
append_if [expr $use_usb] config_of_app {
			<input label="usb_hid" />}
append config_of_app {
		</config>
		<route> }
append_if [expr $use_ps2] config_of_app {
			<service name="Input">
				<if-arg key="label" value="ps2" /> <child name="ps2_drv" />
			</service> }
append_if [expr $use_usb] config_of_app {
			<service name="Input">
				<if-arg key="label" value="usb_hid" /> <child name="usb_drv" />
			</service> }
append config_of_app {
			<any-service> <parent /> <any-child /> </any-service>
		</route>
	</start>

	<!--
		Note: to get network access in the VM, the virtual network adapter
		      needs to be enabled in the .vbox file (disabled by default)
	-->
	<start name="nic_drv" priority="-1">
		<resource name="RAM" quantum="8M" />
		<provides>
			<service name="Nic" />
		</provides>
	</start>

	<!--
		Note: to use audio in the VM, the virtual audio adapter
		needs to be enabled in the .vbox file (disabled by default).
		(Recording is configured to use the external mic.)
	-->
	<start name="audio_drv">
		<resource name="RAM" quantum="8M"/>
		<provides>
			<service name="Audio_out"/>
			<service name="Audio_in"/>
		</provides>
		<config recording="yes">
			<mixer field="outputs.master" value="255"/>
			<mixer field="record.adc-0:1_source" value="sel2"/>
			<mixer field="record.adc-0:1" value="255"/>
		</config>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<rom>
				<policy label="vbox_pointer -> hover" report="nitpicker -> hover"/>
				<policy label="vbox_pointer -> xray"  report="nitpicker -> xray"/>
				<policy label="vbox_pointer -> shape" report="vbox -> shape"/>
				<policy label="vbox -> usb_devices"   report="usb_drv -> devices"/>
			</rom>
		</config>
	</start>

	<start name="nitpicker" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides><service name="Nitpicker"/></provides>
		<route>
			<service name="Framebuffer"> <child name="fb_drv" /> </service>
			<service name="Input"> <child name="input_merger" /> </service>
			<service name="Report"> <child name="report_rom" /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
		<config>
			<report focus="yes" hover="yes" xray="yes" />

			<domain name="pointer" layer="1" xray="no" origin="pointer" />
			<domain name="vbox"    layer="2" />
			<domain name=""        layer="2" />

			<policy label="vbox_pointer" domain="pointer"/>
			<policy label="nit_fb"       domain="vbox"/>
			<policy label=""             domain=""/>

			<global-key name="KEY_SCROLLLOCK" operation="xray" />
			<global-key name="KEY_SYSRQ"      operation="kill" />
			<global-key name="KEY_PRINT"      operation="kill" />
			<global-key name="KEY_F11"        operation="kill" />
			<global-key name="KEY_F12"        operation="xray" />
		</config>
	</start>

	<start name="vbox_pointer" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<route>
			<service name="Nitpicker"> <child name="nitpicker"/>  </service>
			<service name="ROM">       <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nit_fb" priority="-1">
		<resource name="RAM" quantum="8M" />
		<provides>
			<service name="Framebuffer" />
			<service name="Input" />
		</provides>
	</start>

	<start name="vbox" priority="-2">
		<binary name="virtualbox" />
		<resource name="RAM" quantum="1280M"/>}
append config_of_app "
			<config ld_verbose=\"yes\" vbox_file=\"${vbox_file}\" vm_name=\"${flavor}\">"
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
			<service name="Framebuffer"> <child name="nit_fb" /> </service>
			<service name="Input"> <child name="nit_fb" /> </service>
			<service name="Report"><child name="report_rom" /></service>
			<service name="ROM">
				<if-arg key="label" value="usb_devices"/> <child name="report_rom"/>
			</service>
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
