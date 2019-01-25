proc append_socket_fs_build_components { } {
	global build_components
	append build_components { lib/vfs/lwip }
}

proc socket_fs_plugin {} { return lwip }

proc append_socket_fs_boot_modules {} {
	global boot_modules
	append boot_modules { vfs_lwip.lib.so }
}
