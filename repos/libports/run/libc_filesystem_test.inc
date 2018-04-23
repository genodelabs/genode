#
# \brief  Test for using file systems in the libc
# \author Christian Prochaska
# \date   2011-05-27
#

if {[have_spec odroid_xu] || [have_spec zynq]} {
	puts "Run script does not support this platform."
	exit 0
}

# use SD on ARM
set use_sd_card_drv [expr [have_spec omap4] || [have_spec arndale] || [have_spec pl180]]

# use AHCI on x86
set use_ahci        [expr [have_spec x86] && ![have_spec linux]]

# use ram_blk on Linux
set use_ram_blk     [have_spec linux]

if {[catch { exec which $mkfs_cmd } ]} {
	puts stderr "Error: $mkfs_cmd not installed, aborting test"; exit }

if {[expr ![have_include "power_on/qemu"] && !$use_ram_blk]} {
	puts "\nPlease setup your native sd or hard drive. Remove this fail stop";
	puts "check when you have prepared your native environment.\n";
	exit 0
}

#
# Build
#

set build_components {
	core init
	drivers/timer
	test/libc_block
}

lappend build_components test/libc_$filesystem

lappend_if $use_ahci        build_components drivers/ahci
lappend_if $use_sd_card_drv build_components drivers/sd_card
lappend_if $use_ram_blk     build_components server/ram_blk

source ${genode_dir}/repos/base/run/platform_drv.inc
append_platform_drv_build_components

build $build_components

create_boot_directory

#
# Generate config
#

set config {
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
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>}
append config "
	<start name=\"test-libc_$filesystem\">"
append config {
		<resource name="RAM" quantum="8M"/>
		<config>
			<iterations value="1"/>}
append_if [have_include "power_on/qemu"] config {
			<write-read rounds="2" size="1M" buffer_size="8K"/>}
append config {
			<libc stdout="/dev/log" stderr="/dev/log"/>
			<vfs>
				<dir name="dev">}
append config $libc_dev_blkdev
append config {
					<log/>
				</dir>
			</vfs>
		</config>
	</start>}

append_platform_drv_config

append_if $use_ahci config {
	<start name="ahci_drv">
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<any-service> <parent /> <any-child /></any-service>
		</route>
		<config>}
append_if $use_ahci config "
			<policy label_prefix=\"test-libc_$filesystem\" device=\"0\" writeable=\"yes\"/>"
append_if $use_ahci config {
		</config>
	</start>}

append_if $use_sd_card_drv config {
	<start name="sd_card_drv">
		<resource name="RAM" quantum="1M" />
		<provides><service name="Block"/></provides>
	</start>
}

append_if $use_ram_blk config {
	<start name="ram_blk">
		<resource name="RAM" quantum="128M" />
		<provides><service name="Block"/></provides>
		<config file="test.hda" block_size="512"/>
	</start>}

append config {
</config>
}

install_config $config

#
# Boot modules
#

# generic modules
set boot_modules {
	core init timer
	ld.lib.so libc.lib.so vfs.lib.so libm.lib.so posix.lib.so
}

append boot_modules test-libc_$filesystem
append boot_modules " "
append boot_modules libc_$filesystem.lib.so

lappend_if $use_ahci         boot_modules ahci_drv
lappend_if $use_sd_card_drv  boot_modules sd_card_drv
lappend_if $use_ram_blk      boot_modules ram_blk
lappend_if $use_ram_blk      boot_modules test.hda

append_platform_drv_boot_modules

build_boot_image $boot_modules

#
# Execute test case
#

set disk_image "bin/test.hda"
set cmd "dd if=/dev/zero of=$disk_image bs=1024 count=65536"
puts "creating disk image: $cmd"
catch { exec sh -c $cmd }

set cmd "$mkfs_cmd $mkfs_opts $disk_image"
puts "formating disk: $cmd"
catch { exec sh -c $cmd }

#
# Qemu
#
append qemu_args "  -nographic "
append_if $use_ahci        qemu_args " -drive id=disk,file=$disk_image,format=raw,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -boot d"
append_if $use_sd_card_drv qemu_args " -drive file=$disk_image,format=raw,if=sd,cache=writeback "

run_genode_until ".*child \"test-libc_$filesystem\" exited with exit value 0.*" 120

exec rm -f $disk_image

# vi: set ft=tcl :
