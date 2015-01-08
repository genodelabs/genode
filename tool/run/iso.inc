##
# Install files needed to create a bootable ISO image
#
# The ISO boot concept uses isolinux to load GRUB, which in turn loads Genode.
# This way we can make use of isolinux' support for booting ISO images from a
# USB stick.
#
proc install_iso_bootloader_to_run_dir { } {
	puts "install bootloader"

	exec mkdir -p [run_dir]/boot/isolinux
	exec cp [genode_dir]/tool/boot/chain.c32    [run_dir]/boot/isolinux
	exec cp [genode_dir]/tool/boot/isolinux.bin [run_dir]/boot/isolinux
	exec cp [genode_dir]/tool/boot/isolinux.cfg [run_dir]/boot/isolinux

	exec mkdir -p [run_dir]/boot/grub
	exec cp [genode_dir]/tool/boot/stage2_eltorito [run_dir]/boot/grub

	exec cp [genode_dir]/tool/boot/bender [run_dir]/boot/bender
}
