proc append_socket_fs_build_components { } {
	global build_components
	append build_components { lib/vfs/lxip }
}

proc socket_fs_plugin {} { return lxip }

proc append_socket_fs_boot_modules {} {
	global boot_modules
	append boot_modules { lxip.lib.so vfs_lxip.lib.so }
}
