assert_spec x86

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
                  [depot_user]/pkg/usb_webcam \
                  [depot_user]/src/acpi_drv \
                  [depot_user]/src/dynamic_rom \
                  [depot_user]/src/init \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/pc_usb_host_drv \
                  [depot_user]/src/pci_decode \
                  [depot_user]/src/platform_drv \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/rom_reporter \
                  [depot_user]/src/vesa_drv

import_from_depot $test_imports

append config {
<config verbose="no" prio_levels="2">
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

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
		<route>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="pci_decode -> system"    report="acpi_drv -> acpi"/>
			<policy label="platform_drv -> devices" report="pci_decode -> devices"/>
		</config>
	</start>

	<start name="acpi_drv" caps="350">
		<resource name="RAM" quantum="4M"/>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="pci_decode" caps="350">
		<resource name="RAM" quantum="2M"/>
		<route>
			<service name="Report">             <child name="report_rom"/> </service>
			<service name="ROM" label="system"> <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="platform_drv" caps="100" managing_system="yes">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Platform"/>
		</provides>
		<route>
			<service name="ROM" label="devices"> <child name="report_rom"/> </service>
			<service name="Timer">   <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
		<config>
			<policy label="usb_drv -> " info="yes"> <pci class="USB"/> </policy>
			<policy label="fb_drv -> "  info="yes"> <pci class="VGA"/> </policy>
		</config>
	</start>

	<start name="usb_drv" priority="0" caps="200">
		<binary name="pc_usb_host_drv"/>
		<resource name="RAM" quantum="16M"/>
		<!-- <resource name="CPU" quantum="10"/> -->
		<provides><service name="Usb"/></provides>
		<config bios_handoff="no">
			<policy label_suffix="usb_webcam -> ">
				<device vendor_id="} [libuvc_vendor_id] {" product_id="} [libuvc_product_id] {"/>
			</policy>
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
			<service name="Report"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>

	<start name="test_config" priority="-1">
		<binary name="dynamic_rom"/>
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="ROM"/> </provides>
		<config verbose="yes">
			<rom name="config">
				<inline>
					<config verbose="no">
						<parent-provides>
							<service name="ROM"/>
							<service name="PD"/>
							<service name="CPU"/>
							<service name="LOG"/>
							<service name="Timer"/>
							<service name="Gui"/>
							<service name="Capture"/>
						</parent-provides>
						<start name="} $test_binary {" caps="100">
							<resource name="RAM" quantum="15M"/>
							<config period_ms="20" width="640" height="480"> }
append config $test_vfs_config
append config {
								<view xpos="100" ypos="100"/> <!--view xpos="800" ypos="600"/-->
							</config>
							<route>
								<any-service> <parent/> </any-service>
							</route>
						</start>
					</config>
				</inline>
				<sleep milliseconds="7000"/>
				<inline/>
				<sleep milliseconds="3000"/>
			</rom>
		</config>
		<route>
			<service name="Timer"> <child name="timer"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="test-capture" priority="-1" caps="200">
		<binary name="init"/>
		<resource name="RAM" quantum="16M"/>
		<route>
			<service name="ROM" label="config">
				<child name="test_config" label="config"/>
			</service>
			<service name="Timer"> <child name="timer"/> </service>
			<service name="Gui"> <child name="nitpicker"/> </service>
			<service name="Capture"> <child name="webcam"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="webcam" priority="-1" caps="800">
		<binary name="init"/>
		<resource name="RAM" quantum="64M"/>
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

build_boot_image { }

append qemu_args { -usb -device usb-host,vendorid=[libuvc_vendor_id],productid=[libuvc_product_id] }

run_genode_until forever
