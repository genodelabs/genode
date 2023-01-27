##
# Determine GRUB2 directory
#
proc get_grub2_dir { } {
	set ::env(MAKEFLAGS) s
	set grub2_path [exec [genode_dir]/tool/ports/current grub2]

	if {![file exists $grub2_path]} {
		puts ""
		puts "Port not prepared or outdated:"
		puts "  grub2"
		puts ""
		puts "You can prepare respectively update it as follows:"
		puts "  [genode_dir]/tool/ports/prepare_port grub2"
		puts ""
		exit 1
	}

	return $grub2_path
}


proc create_header_grub2_config { } {
	#
	# Generate GRUB2 config file
	#
	set fh [open "[run_dir]/boot/grub/grub.cfg" "WRONLY CREAT TRUNC"]

	# If timeout != 0 is used a font is required in graphic mode
	puts $fh "loadfont /boot/font.pf2"
	puts $fh "set timeout=0"

	# choose best graphic mode
	# uncomment line to get vga console working in hypervisor
	puts $fh "set gfxpayload=auto"

	if {[have_include image/uefi]} {
		puts $fh "insmod gfxterm"
		puts $fh "terminal_output gfxterm"
		puts $fh "insmod gfxterm_background"
		puts $fh "insmod png"
		puts $fh "background_image -m center /boot/boot.png"
	}

	# set this to get text console instead of graphical console
	# puts $fh "terminal_input console"
	# puts $fh "terminal_output console"

	return $fh
}
