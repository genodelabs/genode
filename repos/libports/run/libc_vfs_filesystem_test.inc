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

assert {[have_include power_on/qemu] || [have_spec linux]}

if {[catch { exec which $mkfs_cmd } ]} {
	puts stderr "Error: $mkfs_cmd not installed, aborting test"; exit }

#
# Build
#

set build_components { lib/libc lib/vfs test/libc_vfs }

lappend build_components {*}$test_build_components

build $build_components

create_boot_directory

lappend depot_pkgs [depot_user]/src/[base_src] \
                   [depot_user]/src/init \
                   [depot_user]/src/vfs_block \
                   [depot_user]/src/vfs_import

lappend_if $use_vfs_server depot_pkgs [depot_user]/src/vfs

import_from_depot {*}$depot_pkgs

#
# Generate config
#

proc vfs_server_vfs_fs_node { } {
	global  test_vfs_config
	return $test_vfs_config
}

proc vfs_server_start_node { } { return [join [list {
+ start vfs | caps: 200 | ram: 24M
  + provides | + service File_system
  + config
    + policy | label_prefix: test-libc_vfs | root: / | writeable: yes
    + vfs
      + dir dev | + log
      + } [vfs_server_vfs_fs_node] {
} ] ""] }

proc test_vfs_fs_node { } { return "fs" }

if {!$use_vfs_server} {
	proc vfs_server_start_node { } { }
	proc test_vfs_fs_node { } {
		global  test_vfs_config
		return $test_vfs_config
	}
}

if {[have_include "power_on/qemu"]} {
	proc test_command { } { return "write-read | size: 1M | buffer_size: 8K" }
} else {
	proc test_command { } { }
}

install_config {
config | verbose: yes
+ parent-provides
  + service CPU
  + service IO_MEM
  + service IO_PORT
  + service IRQ
  + service LOG
  + service PD
  + service RM
  + service ROM

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 200 | ram: 2M

+ start timer
  + provides | + service Timer

+ start test-libc_vfs | caps: 200 | ram: 28M
  + config
    + iterations | value: 1
    + } [test_command] {
    + libc | stdout: /dev/log | stderr: /dev/log
    + vfs
      + dir dev | + log
      + } [test_vfs_fs_node] {

+ start vfs_block | caps: 700 | ram: 80M
  + provides | + service Block
  + config
    + default-policy | file: /test.hda | block_size: 512 | writeable: yes
    + vfs
      + ram
      + import | + rom test.hda

} [vfs_server_start_node] {
-
}

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

run_genode_until ".*child \"test-libc_vfs\" exited with exit value 0.*" 100

exec rm -f $disk_image

# vi: set ft=tcl :
