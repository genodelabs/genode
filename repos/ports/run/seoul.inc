#
# \brief  Using Seoul on Genode
# \author Norman Feske
# \author Markus Partheymueller
# \author Alexander Boettcher
# \date   2011-11-21

set use_fs_rump $use_block_vdi
set use_drv_ahci [expr $use_block_vdi || $use_block_sata]
set use_vfs_block [expr $use_block_ram || $use_genode_iso]

create_boot_directory

import_from_depot [depot_user]/src/[base_src] \
                  [depot_user]/src/init \
                  [depot_user]/src/nitpicker

if {$use_fs_rump} {
	import_from_depot [depot_user]/src/vfs \
	                  [depot_user]/src/rump
}

if {$use_part_block} {
	import_from_depot [depot_user]/src/part_block
}

#
# Build
#

assert_spec x86

set map_small         "no"
set vmm_vcpu_same_cpu "no"

if {[have_spec sel4]} {
	set map_small  "yes"
	set vmm_vcpu_same_cpu  "yes"

	# seL4 has no AMD SVM support
	if {[have_include "power_on/qemu"]} {
		puts "\n Run script is not supported on this platform. \n";
		exit 0
	}
}

if {[have_spec foc]} {
	# The performance is considerable bad when
	# vmm and vcpu is not on same physical CPU
	set vmm_vcpu_same_cpu  "yes"

	# Qemu SVM has no EPT support
	if {[have_include "power_on/qemu"]} {
		puts "\n Run script is not supported on this platform. \n";
		exit 0
	}
}

source ${genode_dir}/repos/base/run/platform_drv.inc
# override defaults of platform_drv.inc
proc platform_drv_priority {} { return { priority="-1"} }

set build_components {
	drivers/rtc
	drivers/ps2
	app/seoul
}

if {$use_fancy_stuff} { set use_framebuffer 1 }

lappend_if $use_block_vdi       build_components server/vdi_block
lappend_if $use_usb             build_components drivers/usb
lappend_if $use_vfs_block       build_components server/vfs_block
lappend_if $use_drv_ahci        build_components drivers/ahci
lappend_if $use_nic_session     build_components drivers/nic
lappend_if $use_nic_session     build_components server/nic_router
lappend_if $use_framebuffer     build_components drivers/framebuffer
lappend_if $use_fancy_stuff     build_components app/status_bar
lappend_if $use_fancy_stuff     build_components app/launchpad
lappend_if $use_fancy_stuff     build_components server/report_rom
lappend_if $use_genode_iso      build_components server/iso9660
lappend_if $use_top             build_components app/top

append_platform_drv_build_components

build $build_components

# write Seoul config file
set vm_cfg_fd [open "bin/vm_seoul.cfg" w]
puts $vm_cfg_fd "<config map_small=\"$map_small\" vmm_vcpu_same_cpu=\"$vmm_vcpu_same_cpu\" vmm_memory=\"16M\">"
puts $vm_cfg_fd {	 <machine verbose="no">
		<mem start="0x0" end="0x9a000"/>
		<mem start="0x100000" end="0xfffff000"/>
		<!--<ioio/>-->
		<nullio io_base="0x80" />
		<pic io_base="0x20" elcr_base="0x4d0"/>
		<pic io_base="0xa0" irq="2" elcr_base="0x4d1"/>
		<pit io_base="0x40" irq="0"/>
		<scp io_port_a="0x92" io_port_b="0x61"/>
		<kbc io_base="0x60" irq_kbd="1" irq_aux="12"/>
		<keyb ps2_port="0" host_keyboard="0x10000"/>
		<mouse ps2_port="1" host_mouse="0x10001"/>
		<rtc io_base="0x70" irq="8"/>
		<serial io_base="0x3f8" irq="0x4" host_serial="0x4711"/>
		<hostsink host_dev="0x4712" buffer="80"/>
		<vga io_base="0x03c0"/>}

if {!$use_multiboot} {
	puts $vm_cfg_fd {
		<vbios_disk/>
	}
}

	puts $vm_cfg_fd {
		<vbios_keyboard host_keyboard="0x10000"/>
		<vbios_mem/>
		<vbios_time/>
		<vbios_reset/>}

if {$use_multiboot} {
	if {[info exists use_multiboot_modaddr]} {
		puts $vm_cfg_fd "		<vbios_multiboot modaddr=\"$use_multiboot_modaddr\"/>"
	} else {
		puts $vm_cfg_fd {		<vbios_multiboot/>}
	}
} 

puts $vm_cfg_fd {
		<msi/>
		<ioapic/>
		<pcihostbridge bus_num="0" bus_count="0x10" io_base="0xcf8"
		               mem_base="0xe0000000"/>
		<pmtimer io_port="0x8000"/>}

for {set i 0} {$i < $vcpus_to_be_used} {incr i 1} {
	puts $vm_cfg_fd {
		<vcpu/> <halifax/> <vbios/> <lapic/>}
}

if {!$use_multiboot} {

	if {$use_model_ahci} {
		puts $vm_cfg_fd {
		<ahci mem="0xe0800000" irq="14" bdf="0x30"/>
		<drive sigma0drive="0" controller="0" port="0"/>
		}
	}
	if {$use_model_ide} {
		puts $vm_cfg_fd {
		<ide port0="0x1f0" port1="0x3f6" irq="14" bdf="0x38" disk="0"/>
		}
	}
}

if {$use_nic_session} {
	puts $vm_cfg_fd {
		<!-- <rtl8029 irq="9" port="0x300"/> -->
		<intel82576vf/>
	}
}

puts $vm_cfg_fd {
	</machine>
	<multiboot>}

if {$use_multiboot} {
	puts $vm_cfg_fd $multiboot_files
}

puts $vm_cfg_fd {
	 </multiboot>
</config>}
close $vm_cfg_fd

#
# Generate Genode config
#

set config {
<config verbose="yes" prio_levels="4">
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="VM"/>
		<service name="LOG"/>}

append_if $use_top config {
		<service name="TRACE"/>}

append config {
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>

	<start name="rtc_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Rtc"/></provides>
	</start>}

append_if [expr !$use_usb] config {
	<start name="ps2_drv" priority="-1">
		<resource name="RAM" quantum="3M"/>
		<config/>
		<route> <any-service><parent/> <any-child/> </any-service> </route>
	</start> }

append_if $use_drv_ahci config {
	<start name="ahci_drv" priority="-1">
		<resource name="RAM" quantum="1M" />
		<provides><service name="Block"/></provides>
		<config>}

append_if [expr $use_block_sata && !$use_part_block] config {
			<policy label="seoul -> VirtualDisk 0" device="0" writeable="yes"/>}

append_if [expr $use_block_vdi && !$use_part_block] config {
			<policy label="rump_fs -> " device="0" writeable="yes"/>}

append_if [expr $use_block_vdi && $use_part_block] config {
			<policy label="part_block -> " device="0" writeable="yes"/>}

append_if [expr $use_block_sata && $use_part_block] config {
			<policy label="part_block -> " device="0" writeable="yes"/>}

append_if $use_drv_ahci config {
		</config>
	</start>}

append_if $use_part_block config {
	<start name="part_block" priority="-1">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Block"/></provides>
		<route>
			<any-service><child name="ahci_drv"/> <parent/></any-service>
		</route>
		<config>
			<policy label="seoul -> VirtualDisk 0" partition="5" writeable="yes"/>
			<policy label="rump_fs -> " partition="4" writeable="yes"/>
		</config>
	</start>}

append_if $use_block_ram config {
	<start name="vfs_block" priority="-1">
		<resource name="RAM" quantum="266M" />
		<provides><service name="Block"/></provides>
		<config>
			<vfs>
				<ram/>
				<import>
					<rom name="seoul-disc.raw"/>
				</import>
			</vfs>
			<default-policy file="/seoul-disc.raw" block_size="512"
			                writeable="yes"/>
		</config>
	</start>}

append_if $use_fs_rump config {
	<start name="rump_fs" priority="-1" caps="200">
		<binary name="vfs"/>
		<resource name="RAM" quantum="32M"/>
		<provides><service name="File_system"/></provides>
		<config ld_verbose="yes">
			<vfs>
				<rump fs="ext2fs" ram="28M"/>
			</vfs>
			<default-policy root="/" writeable="yes"/>
		</config>
		<route>
			<service name="Timer"><child name="timer"/></service>}

append_if [expr $use_fs_rump && $use_part_block] config {
			<service name="Block"><child name="part_block"/></service>}
append_if [expr $use_fs_rump && !$use_part_block] config {
			<service name="Block"><child name="ahci_drv"/></service>}

append_if $use_fs_rump config {
			<any-service> <parent/> </any-service>
		</route>
	</start>}

append_if $use_block_vdi config {
	<start name="vdi_block">
		<resource name="RAM" quantum="8M"/>
		<provides> <service name="Block"/> </provides>
		<config file="/seoul_stress32.vdi" writeable="yes">
			<vfs> <fs buffer_size="1M"/> </vfs>
			<policy label="seoul -> VirtualDisk 0" writeable="yes"/>
		</config>
		<route>
			<service name="File_system"><child name="rump_fs"/></service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if $use_genode_iso config {
	<start name="vfs_block" priority="-1">
		<resource name="RAM" quantum="16M" />
		<provides><service name="Block"/></provides>
		<config>
			<vfs>
				<rom name="genode.iso"/>
			</vfs>
			<default-policy file="/genode.iso" block_size="2048"/>
		</config>
	</start>

	<start name="iso9660" priority="-1">
		<resource name="RAM" quantum="16M"/>
		<provides><service name="ROM"/></provides>
		<route>
			<service name="Block"><child name="vfs_block"/></service>
			<any-service><parent/></any-service>
		</route>
	</start>}

append_platform_drv_config

append_if $use_nic_session config {

	<start name="nic_drv" priority="-2">
		<binary name="ipxe_nic_drv"/>
		<resource name="RAM" quantum="8M"/>
		<config mode="uplink_client"/>
		<route>
			<service name="Uplink"><child name="nic_router"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>

	<start name="nic_router" caps="200">
		<resource name="RAM" quantum="10M"/>
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="yes">

			<policy label_prefix="seoul"      domain="downlink"/>
			<policy label_prefix="launchpad"  domain="downlink"/>
			<policy label_prefix="nic_drv"    domain="uplink"/>

			<domain name="uplink">

				<nat domain="downlink"
				     tcp-ports="16384"
				     udp-ports="16384"
				     icmp-ids="16384"/>

			</domain>

			<domain name="downlink" interface="10.0.3.1/24">

				<dhcp-server ip_first="10.0.3.55" ip_last="10.0.3.155" dns_server_from="uplink"/>

				<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>
				<udp dst="0.0.0.0/0"><permit-any domain="uplink" /></udp>
				<icmp dst="0.0.0.0/0" domain="uplink"/>

			</domain>

		</config>
	</start>
}

append_if $use_framebuffer config {
	<start name="fb_drv" priority="-1" caps="130">
		<binary name="vesa_fb_drv"/>
		<resource name="RAM" quantum="16M"/>
		<route>
			<service name="Timer">    <child name="timer"/></service>
			<service name="Capture">  <child name="nitpicker"/></service>
			<service name="Platform"> <any-child/></service>
			<any-service> <parent/> </any-service>
		</route>}
append_if [expr $use_framebuffer &&  [have_include "power_on/qemu"]] config {
		<config width="1280" height="960" depth="32" buffered="yes"/>}
append_if [expr $use_framebuffer && ![have_include "power_on/qemu"]] config {
		<config buffered="yes"/>}
append_if $use_framebuffer config {
	</start> }

if {!$use_fancy_stuff} {
append config {
	<start name="seoul" priority="-3" caps="800">
		<binary name="seoul"/>}
append config "
		<resource name=\"RAM\" quantum=\"$memory_vmm_vm\"/>"
append config {
		<route>
			<service name="Timer"><child name="timer"/></service>
			<service name="ROM" label="config">
				<parent label="vm_seoul.cfg"/> </service>}
append_if [expr $use_nic_session] config {
			<service name="Nic"> <child name="nic_router"/> </service>}
append_if $use_genode_iso config {
			<service name="ROM" unscoped_label="seoul"> <parent/> </service>
			<service name="ROM" unscoped_label="ld.lib.so"> <parent/> </service>
			<service name="ROM" label="platform_info"> <parent/> </service>
			<service name="ROM"><child name="iso9660"/></service>}
append_if $use_block_vdi config {
			<service name="Block"><child name="vdi_block"/></service>}
append_if [expr $use_block_sata && $use_part_block] config {
			<service name="Block"><child name="part_block"/></service>}
append config {
			<service name="Rtc"><child name="rtc_drv"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start> }
}

append_if $use_usb config {
	<start name="usb_drv" priority="-1" caps="120">
		<resource name="RAM" quantum="12M"/>
		<config uhci="yes" ehci="yes" xhci="yes">
			<hid/>
		</config>
	</start>}

append config {
	<start name="nitpicker" priority="-1" caps="120">
		<resource name="RAM" quantum="8M"/>
		<provides>
			<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
		</provides>
		<config>
			<capture/> <event/>
			<report focus="yes" />
			<domain name="pointer" layer="1" content="client" label="no" origin="pointer" />
			<domain name="panel"   layer="2" content="client" label="no" focus="none" />
			<domain name="init.1"  layer="3" content="client" focus="click"
			                       hover="always" xpos="20" ypos="30" height="-20"/>
			<domain name="init.2"  layer="3" content="client" focus="click"
			                       hover="always" xpos="30" ypos="40" height="-20"/>
			<domain name="init.3"  layer="3" content="client" focus="click"
			                       hover="always" xpos="40" ypos="50" height="-20"/>
			<domain name=""        layer="3" content="client" focus="click"
			                       hover="always" xpos="10" ypos="20" height="-20"/>
			<policy label_prefix="pointer"    domain="pointer"/>
			<policy label_prefix="status_bar" domain="panel"/>

			<policy label_prefix="launchpad -> init.1" domain="init.1"/>
			<policy label_prefix="launchpad -> init.2" domain="init.2"/>
			<policy label_prefix="launchpad -> init.3" domain="init.3"/>

			<default-policy domain=""/>
		</config>
		<route>
			<service name="Timer">  <child name="timer"/></service>
			<service name="Report"> <child name="report_rom"/></service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>

	<start name="pointer">
		<resource name="RAM" quantum="2M"/>
		<config/>
	</start>

	<start name="report_rom">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="status_bar -> focus" report="nitpicker -> focus"/>
		</config>
	</start>}

append_if $use_top config {
	<start name="top">
		<resource name="RAM" quantum="2M"/>
		<config period_ms="10000"/>
	</start>}

append_if $use_fancy_stuff config {
	<start name="status_bar">
		<resource name="RAM" quantum="1M"/>
		<route>
			<service name="ROM" label="focus"> <child name="report_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
	<start name="launchpad" priority="-2" caps="1500">
		<resource name="RAM" quantum="64000M"/>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>
			<service name="ROM" label="config">
				<parent label="launchpad-config"/> </service>
			<any-service><parent/><any-child/></any-service>
		</route>
	</start>}

append config {
</config>}

#
# Generate Launchpad config file
#
if {$use_fancy_stuff} {
	set launchpad_cfg_fd [open "bin/launchpad-config" w]

	puts $launchpad_cfg_fd "<config>
	<launcher ram_quota=\"$memory_init\" name=\"init\" caps=\"400\" >"

	puts $launchpad_cfg_fd {
		<config>
			<parent-provides>
				<service name="ROM"/>
				<service name="IRQ"/>
				<service name="IO_MEM"/>
				<service name="IO_PORT"/>
				<service name="PD"/>
				<service name="RM"/>
				<service name="CPU"/>
				<service name="LOG"/>
				<service name="Gui"/>
				<service name="Timer"/>
				<service name="Nic"/>
				<service name="Block"/>
				<service name="Rtc"/>
				<service name="VM"/>
			</parent-provides>

			<start name="seoul" priority="-1" caps="200">
				<binary name="seoul"/>
				<resource name="RAM" quantum="256M"/>
				<route>
					<service name="Timer"><parent/></service>
					<service name="Nic"><parent/></service>
					<service name="ROM" label="config">
						<parent label="vm_seoul.cfg"/> </service>
					<any-service><parent/></any-service>
				</route>
			</start>
		</config>
	</launcher>}
	puts $launchpad_cfg_fd {</config>}
	close $launchpad_cfg_fd
}

install_config $config

#
# Boot modules
#

# generic modules
set boot_modules {
	rtc_drv
	seoul
	vm_seoul.cfg
}

lappend_if $use_block_vdi     boot_modules vdi_block
lappend_if [expr !$use_usb]   boot_modules ps2_drv
lappend_if $use_usb           boot_modules usb_drv
lappend_if $use_vfs_block     boot_modules vfs_block
lappend_if $use_vfs_block     boot_modules vfs.lib.so
lappend_if $use_drv_ahci      boot_modules ahci_drv
lappend_if $use_nic_session   boot_modules ipxe_nic_drv
lappend_if $use_nic_session   boot_modules nic_router
lappend_if $use_framebuffer   boot_modules vesa_fb_drv

lappend_if $use_fancy_stuff   boot_modules status_bar
lappend_if $use_fancy_stuff   boot_modules launchpad
lappend_if $use_fancy_stuff   boot_modules launchpad-config
lappend_if $use_fancy_stuff   boot_modules report_rom

lappend_if $use_genode_iso    boot_modules iso9660
lappend_if $use_genode_iso    boot_modules genode.iso

lappend_if $use_block_ram     boot_modules seoul-disc.raw

lappend_if $use_top           boot_modules top

#
# Add OS binaries of guest
#

if {$use_multiboot} {
	set guest_os_binary_missing 0
	set binary_counter 0
	foreach binary $guest_os_binaries {
		if {![file exists bin/$binary]} {
			puts stderr "Error: guest OS binary \"bin/$binary\" does not exist"
			set guest_os_binary_missing 1
		}

		if {[info exists sha1_os_binaries]} {
			set sha1 [exec sha1sum bin/$binary]
			set sha1 [regexp -inline {[0-9a-h]+} $sha1]
			if {[string compare $sha1 [lindex $sha1_os_binaries $binary_counter]]} {
				puts "SHA1 sum of binary does not match with expected one - abort"
				puts "$binary $sha1 != [lindex $sha1_os_binaries $binary_counter]"
				set guest_os_binary_missing 1
			}
		}
		incr binary_counter 1
	}

	if {$guest_os_binary_missing} { exit 1 }

	append boot_modules $guest_os_binaries
}

if {$use_usb} {
	append qemu_args " -usb -usbdevice mouse -usbdevice keyboard "
}

append_platform_drv_boot_modules

build_boot_image $boot_modules

# A copy of the config is placed in the run folder.
exec rm -f bin/vm_seoul.cfg
