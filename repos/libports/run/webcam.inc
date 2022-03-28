assert_spec x86

set build_components { }

# fuji4
proc libuvc_vendor_id {}  { return "0x04f2" }
proc libuvc_product_id {} { return "0xb564" }

# c270
#proc libuvc_vendor_id {}  { return "0x046d" }
#proc libuvc_product_id {} { return "0x0825" }

# quickcam
#proc libuvc_vendor_id {}  { return "0x046d" }
#proc libuvc_product_id {} { return "0x09c1" }

# t470
#proc libuvc_vendor_id {}  { return "0x0bda" }
#proc libuvc_product_id {} { return "0x58db" }



create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/init \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/dynamic_rom \
                  [depot_user]/src/rom_reporter \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/pc_usb_host_drv \
                  [depot_user]/src/vesa_drv \
                  [depot_user]/pkg/usb_webcam

import_from_depot $test_imports

source ${genode_dir}/repos/base/run/platform_drv.inc
append_platform_drv_build_components
build $build_components


append config {
<config verbose="yes" prio_levels="4">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="VM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
}

append_platform_drv_config

append config {
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="usb_drv" priority="0" caps="200">
		<binary name="pc_usb_host_drv"/>
		<resource name="RAM" quantum="16M"/>
		<!-- <resource name="CPU" quantum="10"/> -->
		<provides><service name="Usb"/></provides>
		<config bios_handoff="no">
			<policy label_suffix="usb_webcam -> usb_device"
			        vendor_id="} [libuvc_vendor_id] {" product_id="} [libuvc_product_id] {"/>
		</config>
		<route>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="fb_drv" priority="-1" caps="150">
		<binary name="vesa_fb_drv"/>
		<resource name="RAM" quantum="20M"/>
		<config/>
		<route>
			<service name="Capture"> <child name="nitpicker"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>

	<start name="nitpicker" priority="-1" caps="150">
		<resource name="RAM" quantum="12M"/>
		<provides>
			<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
		</provides>
		<config>
			<capture/>
			<report focus="yes" hover="yes" />
			<domain name="" layer="3" content="client" label="no" focus="click" hover="always"/>
			<default-policy domain=""/>
		</config>
		<route>
			<service name="Report"> <child name="acpi_report_rom"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>

	<start name="} $test_binary {" priority="-1">
		<resource name="RAM" quantum="8M"/>
		<config period_ms="20" width="640" height="480"> }
append config $test_vfs_config
append config {
			<view xpos="100" ypos="100"/> <!--view xpos="800" ypos="600"/-->
		</config>
		<route>
			<service name="Gui"> <child name="nitpicker"/> </service>
			<service name="Capture"> <child name="webcam"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="dynamic-webcam">
		<binary name="dynamic_rom"/>
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="capture">
				<inline>
					<capture enabled="true"/>
				</inline>
				<sleep milliseconds="8000"/>
				<inline>
					<capture enabled="no"/>
				</inline>
				<sleep milliseconds="3000"/>
			</rom>
		</config>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="rom_reporter">
		<resource name="RAM" quantum="1MB"/>
		<provides> <service name="Report"/> </provides>
		<config>
			<rom label="capture"/>
		</config>
		<route>
			<service name="ROM" label="capture"> <child name="dynamic-webcam"/> </service>
			<service name="Report"> <child name="webcam" label="capture"/> </service>
			<any-service> <parent /> </any-service>
		</route>
	</start>

	<start name="webcam" caps="800" priority="-1">
		<binary name="init"/>
		<resource name="RAM" quantum="64MB"/>
		<route>
			<service name="ROM" label="config"> <parent label="usb_webcam.config"/> </service>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Usb"> <child name="usb_drv"/> </service>
			<any-service> <parent /> </any-service>
		</route>
		<provides> <service name="Capture"/> <service name="Report"/> </provides>
	</start>
</config>}

install_config $config

append_platform_drv_boot_modules

append boot_modules { }

build_boot_image $boot_modules

append qemu_args { -usb -device usb-host,vendorid=[libuvc_vendor_id],productid=[libuvc_product_id] }

run_genode_until forever
