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

	set filelist [glob -nocomplain $grub2_path/boot/grub2/*.mod]
	foreach file $filelist {
		file copy $file [run_dir]/boot/grub/i386-pc/.
	}

	exec cp [genode_dir]/tool/boot/bender [run_dir]/boot/bender
	exec cp $grub2_path/boot/font.pf2     [run_dir]/boot/font.pf2
}
