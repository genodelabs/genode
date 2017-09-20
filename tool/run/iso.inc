##
# Install files needed to create a bootable ISO image
#
# The ISO boot concept uses GRUB2 and xorriso to boot from ISO and USB stick.
#

source [genode_dir]/tool/run/grub2.inc

proc install_iso_bootloader_to_run_dir { } {
	puts "install bootloader"

	set grub2_path [get_grub2_dir]

	exec mkdir -p [run_dir]/boot/grub/i386-pc
	exec cp $grub2_path/boot/grub2/eltorito.img [run_dir]/boot/grub/i386-pc/.
	foreach file [glob -nocomplain $grub2_path/boot/grub2/\*.mod] {
		exec cp $file [run_dir]/boot/grub/i386-pc/[file tail $file]
	}

	exec cp [genode_dir]/tool/boot/bender [run_dir]/boot/bender
}
