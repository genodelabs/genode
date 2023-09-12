#
# \brief  Test for block-backed file-systems in the libc VFS
# \author Christian Prochaska
# \author Emery Hemingway
# \date   2017-07-31
#

#
# \arg mkfs_cmd
# \arg mkfs_opts
# \arg use_vfs_server
# \arg test_build_components
# \arg test_vfs_config
#

if {[catch { exec which $mkfs_cmd } ]} {
	puts stderr "Error: $mkfs_cmd not installed, aborting test"; exit }

if {[expr ![have_include "power_on/qemu"] && ![have_spec linux]]} {
	puts "Run script does not support this platform."
	exit 0
}

#
# Build
#

set build_components { test/libc_vfs lib/libc lib/vfs test/libc_vfs }

lappend build_components {*}$test_build_components

build $build_components

create_boot_directory

set depot_pkgs " [depot_user]/src/[base_src]
                 [depot_user]/src/init
                 [depot_user]/src/vfs_block
                 [depot_user]/src/vfs_import "

lappend_if $use_vfs_server depot_pkgs [depot_user]/src/vfs

import_from_depot {*}$depot_pkgs

#
# Generate config
#

set config {
<config verbose="yes">
	<parent-provides>
		<service name="CPU"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="IRQ"/>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="ROM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
	<start name="test-libc_vfs" caps="200">
		<resource name="RAM" quantum="28M"/>
		<config>
			<iterations value="1"/>}
append_if [have_include "power_on/qemu"] config {
			<write-read size="1M" buffer_size="8K"/>}
append config {
			<libc stdout="/dev/log" stderr="/dev/log"/>
			<vfs>
				<dir name="dev"> <log/> </dir>}
if {$use_vfs_server} {
	append config "<fs/>"
} else {
	append config $test_vfs_config
}
append config {
			</vfs>
		</config>
	</start>

	<start name="vfs_block" caps="200">
		<resource name="RAM" quantum="68M" />
		<provides><service name="Block"/></provides>
		<config>
			<vfs>
				<ram/>
				<import>
					<rom name="test.hda"/>
				</import>
			</vfs>
			<default-policy file="/test.hda" block_size="512"
			                writeable="yes"/>
		</config>
	</start>}

append_if $use_vfs_server config "
	<start name=\"vfs\" caps=\"200\">
		<resource name=\"RAM\" quantum=\"24M\" />
		<provides><service name=\"File_system\"/></provides>
		<config>
			<vfs>
				<dir name=\"dev\"> <log/> </dir>}
				$test_vfs_config
			</vfs>
			<policy label_prefix=\"test-libc_vfs\" root=\"/\" writeable=\"yes\"/>
		</config>
	</start>"

append config {
</config>
}

install_config $config

#
# Prepare hard-disk image for test (used in RAM)
#

set disk_image "bin/test.hda"
set cmd "dd if=/dev/zero of=$disk_image bs=1024 count=65536"
puts "creating disk image: $cmd"
catch { exec sh -c $cmd }

set cmd "$mkfs_cmd $mkfs_opts $disk_image"
puts "formating disk: $cmd"
catch { exec sh -c $cmd }

#
# Boot modules
#

build_boot_image [list {*}[build_artifacts] test.hda]

#
# Qemu
#
append qemu_args " -nographic "

run_genode_until ".*child \"test-libc_vfs\" exited with exit value 0.*" 90

exec rm -f $disk_image

# vi: set ft=tcl :
