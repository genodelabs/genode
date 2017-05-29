#
# \brief  Test of Block session interface provided by server/part_blk
#

requires_installation_of parted

#
# Build
#

build {
	core init
	drivers/timer
	server/rom_blk
	server/part_blk
	server/report_rom
	test/blk/cli
}

set img_file ata.$mode.raw
set img_path bin/$img_file

if { ![file exists $img_path] } then {
	set block_count 20480

	# create empty block device file
	catch { exec dd if=/dev/zero of=$img_path bs=512 count=$block_count }

	# create two primary partitions (one is extented) and two logical paritions
	puts "using parted to partition disk image"

	if { $mode == "mbr" } {
		exec parted -s $img_path mklabel msdos
		exec parted -s $img_path mkpart primary fat32  2048s 4095s
		exec parted -s $img_path mkpart extended       4096s 20479s
		exec parted -s $img_path mkpart logical fat32  6144s 10239s
		exec parted -s $img_path mkpart logical fat32 12288s 20479s
	} else {
		exec parted -s $img_path mklabel gpt
		exec parted -s $img_path mkpart one fat32  2048s 4095s
		exec parted -s $img_path mkpart two fat32  4096s 20446s
	}
}

create_boot_directory

#
# Generate config
#

append config {
<config prio_levels="1" verbose="yes">
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
		<provides><service name="Timer"/></provides>
	</start>
	<start name="rom_blk">
		<resource name="RAM" quantum="32M"/>
		<provides><service name="Block"/></provides>
		<config file="} $img_file {" block_size="512"/>
	</start>
	<start name="part_blk">
		<resource name="RAM" quantum="10M" />
		<provides><service name="Block" /></provides>
		<route>
			<any-service><child name="rom_blk"/> <parent/><any-child/></any-service>
		</route>}
if { $mode == "mbr" } {
	append config {
		<config>
			<report partitions="yes"/>
			<policy label_prefix="test-part1" partition="6"/>
			<policy label_prefix="test-part2" partition="1"/>
		</config>}
} else {
	append config {
		<config use_gpt="yes">
			<report partitions="yes"/>
			<policy label_prefix="test-part1" partition="2"/>
			<policy label_prefix="test-part2" partition="1"/>
		</config>}
}

append config {
	</start>
	<start name="report_rom">
		<provides>
			<service name="Report"/>
			<service name="ROM"/>
		</provides>
		<resource name="RAM" quantum="5M" />
		<config verbose="yes"/>
	</start>
	<start name="test-part1">
		<binary name="test-blk-cli"/>
		<resource name="RAM" quantum="5M" />
		<route>
			<any-service> <child name="part_blk" /> <parent/> <any-child/> </any-service>
		</route>
	</start>
	<start name="test-part2">
		<binary name="test-blk-cli"/>
		<resource name="RAM" quantum="5M" />
		<route>
			<any-service> <child name="part_blk" /> <parent/> <any-child/> </any-service>
		</route>
	</start>
</config> }

install_config $config

#
# Boot modules
#

append boot_modules {
	core ld.lib.so init timer
	rom_blk part_blk test-blk-cli report_rom
}
append boot_modules $img_file

build_boot_image $boot_modules

#
# Qemu
#

append qemu_args " -nographic  "

run_genode_until "Tests finished successfully.*\n.*Tests finished successfully.*\n" 100
exec rm $img_path
