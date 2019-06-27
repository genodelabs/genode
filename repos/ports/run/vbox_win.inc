assert_spec x86

if {(![have_spec nova] && ![have_spec foc])} {
	puts "Platform is unsupported."
	exit 0
}

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit 0
}

if {[expr !$use_rumpfs && $use_vms > 1] } {
	puts "\nConfiguration bug - have only one raw block partition.\n"
	exit 1
}

set vdi_image "${flavor}.vdi"
set raw_image "${flavor}.vmdk"
set vbox_file "vm_${flavor}.vbox"
set overlay_image "overlay_${flavor}.vdi"

if {[info exists flavor_extension]} {
	set vbox_file "vm_${flavor}${flavor_extension}.vbox"
}

set build_components {
	server/input_merger
	drivers/nic
	drivers/audio
	server/report_rom
	server/dynamic_rom
}

set boot_modules {
	input_merger
	ipxe_nic_drv
	audio_drv
	report_rom
	dynamic_rom
}

set virtualbox5_binary "virtualbox5"
if {$use_vbox5_nova} { set virtualbox5_binary "virtualbox5-nova" }

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
			<service name="Input" label="ps2"> <child name="ps2_drv" /> </service> }
append_if [expr $use_usb] config_of_app {
			<service name="Input" label="usb_hid"> <child name="usb_drv" /> </service> }
append config_of_app {
			<any-service> <parent /> <any-child /> </any-service>
		</route>
	</start>

	<!--
		Note: to get network access in the VM, the virtual network adapter
		      needs to be enabled in the .vbox file (disabled by default)
	-->
	<start name="nic_drv" priority="-1">
		<binary name="ipxe_nic_drv"/>
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
	<start name="audio_drv" priority="-1">
		<resource name="RAM" quantum="9M"/>
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
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="pointer -> hover"                    report="nitpicker -> hover"/>
			<policy label="pointer -> xray"                     report="nitpicker -> xray"/>}
append config_of_app {
			<policy label="usb_report_filter -> devices"        report="usb_drv -> devices"/>
			<policy label="usb_report_filter -> usb_drv_config" report="usb_drv -> config"/>
			<policy label="vbox1 -> usb_devices"                report="usb_report_filter -> usb_devices"/>}
append_if [expr $use_cpu_load] config_of_app {
			<policy label="cpu_load_display -> trace_subjects"
			        report="trace_subject_reporter -> trace_subjects"/>}
append config_of_app {
		</config>
	</start>

	<start name="nitpicker" priority="-1" caps="150">
		<resource name="RAM" quantum="12M"/>
		<provides><service name="Nitpicker"/></provides>
		<configfile name="nitpicker.config"/>
		<route>
			<service name="Framebuffer"> <child name="fb_drv" /> </service>
			<service name="Input"> <child name="input_merger" /> </service>
			<service name="Report"> <child name="report_rom" /> </service>
			<service name="ROM" label="nitpicker.config"> <child name="dynamic-config"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>

	<start name="pointer" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report"/> </provides>
		<config shapes="yes"/>
		<route>
			<service name="Nitpicker"> <child name="nitpicker"/>  </service>
			<service name="ROM" label="hover"> <child name="report_rom"/> </service>
			<service name="ROM" label="xray"> <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>}

if { $use_vms > 1 } {
	append config_of_app {
	<start name="wm" caps="1000">
		<resource name="RAM" quantum="32M"/>
		<binary name="init"/>
		<provides>
			<service name="Nitpicker"/> <service name="Report"/> <service name="ROM"/>
		</provides>
		<route>
			<service name="ROM" label="config"> <parent label="wm.config"/> </service>
			<service name="Nitpicker"> <child name="nitpicker"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>}
}

append config_of_app {
	<start name="dynamic-config" priority="-1">
		<binary name="dynamic_rom"/>
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="capslock">
				<inline>
					<capslock enabled="yes"/>
				</inline>
				<sleep milliseconds="5000"/>
				<inline>
					<capslock enabled="no"/>
				</inline>
				<sleep milliseconds="5000"/>
			</rom>
			<rom name="nitpicker.config">
				<inline description="standard_mode">
					<config>
						<report focus="yes" hover="yes" />

						<domain name="pointer"  layer="1" content="client" label="no" origin="pointer" />
						<domain name="cpu_load" layer="2" content="client" label="no" />
						<domain name=""         layer="3" content="client" label="no" focus="click" hover="always"/>

						<policy label_prefix="pointer"          domain="pointer"/>
						<policy label_prefix="cpu_load_display" domain="cpu_load"/>
						<default-policy domain=""/>
					</config>
				</inline>
				<sleep milliseconds="50000"/>}

append_if [expr !$use_rumpfs] config_of_app {
				<inline description="shutdown">
					<config>
						<report focus="yes" hover="yes" />

						<domain name="pointer"  layer="1" content="client" label="no" origin="pointer" />
						<domain name="cpu_load" layer="2" content="client" label="no" />
						<domain name=""         layer="3" content="client" label="no" focus="click" hover="always" width="1" height="1" />

						<policy label_prefix="pointer"          domain="pointer"/>
						<policy label_prefix="cpu_load_display" domain="cpu_load"/>
						<default-policy domain=""/>
					</config>
				</inline>
				<sleep milliseconds="500000"/>}

append config_of_app {
			</rom>
		</config>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent /> </any-service>
		</route>
	</start>}

for { set i 1} { $i <= $use_vms } { incr i} {
	append config_of_app "
	<start name=\"vbox${i}\" priority=\"-2\" caps=\"800\">"
	append_if [expr $use_vbox5] config_of_app "
		<binary name=\"$virtualbox5_binary\" />"

	append config_of_app {
		<resource name="RAM" quantum="} $vm_ram {"/>}
	if { $use_vms eq 1 } {
		append config_of_app "
		<config ld_verbose=\"yes\" vbox_file=\"${vbox_file}\" vm_name=\"${flavor}\" xhci=\"yes\" capslock=\"NOROM\">"
	} else {
		append config_of_app "
		<affinity xpos=\"[expr $i]\" width=\"2\" />
		<config ld_verbose=\"yes\" vbox_file=\"${vbox_file}\" vm_name=\"${flavor}\" xhci=\"no\" capslock=\"NOROM\">"
	}
	append config_of_app {
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc"/>
			<vfs>
				<dir name="dev">
					<log/> <rtc/>}

	append_if [expr !$use_rumpfs] config_of_app {
					<block name="sda3" label="raw" block_buffer_count="128" />}
	append config_of_app {
				</dir>}

	append_if [expr $use_ram_fs] config_of_app {
				<dir name="ram">  <fs label="from_ram_fs"/> </dir>}

	append config_of_app "
				<rom name=\"${vbox_file}\"/>"
	append_if [expr !$use_rumpfs] config_of_app "
				<rom name=\"${raw_image}\"/>"

	append_if [expr $use_rumpfs || $use_ram_fs] config_of_app {
				<fs />}

	append config_of_app {
			</vfs>
		</config>
		<route>}

	append_if [expr ($use_vms > 1)] config_of_app {
			<service name="Nitpicker"> <child name="wm"/></service>}

	append_if [expr $use_ram_fs] config_of_app {
			<service name="File_system" label="from_ram_fs"> <child name="ram_fs"/> </service>}

	append_if [expr $use_rumpfs] config_of_app {
				<service name="File_system"> <child name="rump_fs"/> </service>}
	append_if [expr !$use_rumpfs] config_of_app {
				<service name="Block"> <child name="part_block"/> </service>}
	append config_of_app {
			<service name="Report" label="shape"> <child name="pointer"/></service>
			<service name="Report"><child name="report_rom" /></service>
			<service name="ROM" label="usb_devices"> <child name="report_rom" /> </service>
			<service name="ROM" label="capslock"> <child name="dynamic-config"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}
}

source ${genode_dir}/repos/ports/run/virtualbox_auto.inc

# copy vbox configuration to bin directory
exec cp ${genode_dir}/repos/ports/run/${vbox_file} bin/.

if {!$use_rumpfs} {
	exec cp ${genode_dir}/repos/ports/run/${raw_image} bin/.
}

append boot_modules " ${vbox_file} "
append_if [expr !$use_rumpfs] boot_modules " ${raw_image} "

build_boot_image $boot_modules

set wait_timeout 70
if {[have_spec foc]} { set wait_timeout 120 }

run_genode_until {\[init -\> vbox.*].*Guest Additions capability report:.*seamless: no, hostWindowMapping: no, graphics: no} $wait_timeout

# run_genode_until forever 0 [output_spawn_id]

if {$use_rumpfs} {
	set wait_timeout 170
	for { set i 1 } { $i <= $use_vms } { incr i } {
		if { [string match "ubuntu*" $flavor] || [string match "win10*" $flavor] } {
			run_genode_until {\[init -\> vbox.*].*Guest Additions capability report:.*seamless: yes, hostWindowMapping: no, graphics: no} $wait_timeout [output_spawn_id]
		} else {
			run_genode_until {\[init -\> vbox.*].*Guest Additions capability report:.*seamless: yes, hostWindowMapping: no, graphics: yes} $wait_timeout [output_spawn_id]
		}
	}
} else {
	run_genode_until {\[init\] child "vbox1" exited with exit value 0} 60 [output_spawn_id]
	# give block driver bit time to write data to disk
	sleep 5
}

# cleanup bin directory - remove vbox file
exec rm bin/${vbox_file}
if {!$use_rumpfs} {
	exec rm bin/${raw_image}
}
