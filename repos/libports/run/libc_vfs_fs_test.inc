#
# \brief  Test for using the libc_vfs plugin with a file system server
# \author Christian Prochaska
# \date   2017-06-13
#

set use_sd_card_drv [expr [have_spec omap4] || [have_spec arndale] || [have_spec pl180]]
set use_ahci_drv   [expr [have_spec x86] && ![have_spec linux]]
set use_ram_blk    [have_spec linux]
set mkfs [installed_command $mkfs_cmd]

if {[have_spec odroid_xu]} {
        puts "Run script does not support this platform"; exit }

#
# Build
#

set build_components {
	core init
	drivers/timer
	test/libc_vfs
}

lappend build_components $build_component

lappend_if $use_ahci_drv    build_components drivers/ahci
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
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
	<start name="fs">}
append config "
		<binary name=\"$binary\"/>"
append config {
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="File_system"/> </provides>
		<config>
			<default-policy root="/" writeable="yes" />
			<vfs>
				<dir name="dev">
					<log/>}
append config "     $vfs_dev_blkdev"
append config {
				</dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" />
		</config>
	</start>
	<start name="test-libc_vfs">
		<resource name="RAM" quantum="4M"/>
		<config>
			<libc stdout="/dev/log"/>
			<vfs>
				<dir name="dev"> <log/> </dir>
				<fs/>
			</vfs>
		</config>
	</start>}

append_platform_drv_config

append_if $use_ahci_drv config {
	<start name="ahci_drv">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Block"/> </provides>
		<config>
			<policy label_prefix="fs" device="0" writeable="yes" />
		</config>
	</start>
}

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
	test-libc_vfs
}

lappend boot_modules $binary

lappend_if $use_ahci_drv     boot_modules ahci_drv
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

set cmd "$mkfs $mkfs_opts $disk_image"
puts "formating disk image: $cmd"
catch { exec sh -c $cmd }

#
# Qemu
#
append qemu_args "  -nographic "
append_if $use_ahci_drv    qemu_args " -drive id=disk,file=$disk_image,format=raw,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -boot d"
append_if $use_sd_card_drv qemu_args " -drive file=$disk_image,format=raw,if=sd,cache=writeback "

run_genode_until {.*child "test-libc_vfs" exited with exit value 0.*} 120

exec rm -f $disk_image

puts "\nTest succeeded\n"

# vi: set ft=tcl :
