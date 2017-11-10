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
