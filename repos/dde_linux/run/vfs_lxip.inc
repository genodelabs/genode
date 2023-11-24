proc append_socket_fs_build_components { } {
	global build_components
	append build_components { lib/vfs_lxip }
}

proc socket_fs_plugin {} { return lxip }
