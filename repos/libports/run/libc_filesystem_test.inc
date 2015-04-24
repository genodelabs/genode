#
# \brief  Test for using the libc_ffat plugin
# \author Christian Prochaska
# \date   2011-05-27
#

if {[have_spec hw_odroid_xu]} {
	puts "Run script does not support this platform."
	exit 0
}

# use SD on ARM
set use_sd_card_drv [expr [have_spec omap4] || [have_spec platform_arndale] || [have_spec pl180]]

# use AHCI on x86
set use_ahci        [have_spec x86]

if {[catch { exec which $mkfs_cmd } ]} {
	puts stderr "Error: $mkfs_cmd not installed, aborting test"; exit }

if {[have_spec linux]} {
	puts "Run script does not support this platform"; exit }

if {![have_include "power_on/qemu"]} {
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

lappend_if [have_spec  pci] build_components drivers/pci
lappend_if [have_spec acpi] build_components drivers/acpi
lappend_if $use_ahci        build_components drivers/ahci
lappend_if $use_sd_card_drv build_components drivers/sd_card

build $build_components
create_boot_directory

#
# Generate config
#

set config {
<config verbose="yes">
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="CAP"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>}
append config "
	<start name=\"test-libc_$filesystem\">"
append config {
		<resource name="RAM" quantum="8M"/>
		<config>
			<iterations value="1"/>
			<libc stdout="/dev/log" stderr="/dev/log">
				<vfs>
					<dir name="dev">}
append config $libc_dev_blkdev
append config {
						<log/>
					</dir>
				</vfs>
			</libc>
		</config>
	</start>}

append_if [have_spec acpi] config {
	<start name="acpi">
		<resource name="RAM" quantum="8M" constrain_phys="yes"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<config>
			<policy label="acpi_drv">
				<pci class="ALL"/>
			</policy>
			<policy label="ahci">
				<pci class="AHCI"/>
			</policy>
		</config>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if [expr ![have_spec acpi] && [have_spec pci]] config {
	<start name="pci_drv">
		<resource name="RAM" quantum="5M" constrain_phys="yes"/>
		<provides><service name="PCI"/></provides>
		<config>
			<policy label="ahci">
				<pci class="AHCI"/>
			</policy>
		</config>
	</start>}

append_if $use_ahci config {
	<start name="ahci">
		<binary name="ahci" />
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>}

append_if [expr $use_ahci && [have_spec acpi]] config {
			<service name="IRQ"><child name="acpi" /></service>}
append_if $use_ahci config {
			<any-service> <parent /> <any-child /></any-service>
		</route>
	</start>}

append_if $use_sd_card_drv config {
	<start name="sd_card_drv">
		<resource name="RAM" quantum="1M" />
		<provides><service name="Block"/></provides>
	</start>
}

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
	ld.lib.so libc.lib.so
}

append boot_modules test-libc_$filesystem
append boot_modules " "
append boot_modules libc_$filesystem.lib.so

lappend_if [have_spec pci]   boot_modules pci_drv
lappend_if [have_spec acpi]  boot_modules acpi_drv
lappend_if $use_ahci         boot_modules ahci
lappend_if $use_sd_card_drv  boot_modules sd_card_drv

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
append qemu_args " -m 128 -nographic "
append_if $use_ahci        qemu_args " -drive id=disk,file=$disk_image,if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 -boot d"
append_if $use_sd_card_drv qemu_args " -drive file=$disk_image,if=sd,cache=writeback "

run_genode_until ".*child \"test-libc_$filesystem\" exited with exit value 0.*" 60

exec rm -f $disk_image

# vi: set ft=tcl :
