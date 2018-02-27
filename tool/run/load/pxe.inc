##
# Install files needed to boot via PXE
#
proc install_pxe_bootloader_to_run_dir { } {
	exec mkdir -p [run_dir]/boot
	exec cp [genode_dir]/tool/boot/pulsar [run_dir]/boot/pulsar
	exec chmod a+r [run_dir]/boot/pulsar
	exec cp [genode_dir]/tool/boot/bender [run_dir]/boot/bender
	exec chmod a+r [run_dir]/boot/bender
	exec cp [genode_dir]/tool/boot/unzip [run_dir]/boot/unzip
	exec chmod a+r [run_dir]/boot/unzip
}
