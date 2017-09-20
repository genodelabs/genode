##
# Determine GRUB2 directory
#
proc get_grub2_dir { } {
	set ::env(MAKEFLAGS) s
	set grub2_path [exec [genode_dir]/tool/ports/current grub2]

	if {![file exists $grub2_path]} {
		puts ""
		puts "GRUB2 binaries are missing."
		puts "You can download the GRUB2 binaries by invoking:"
		puts "\ttool/ports/prepare_port grub2"
		puts ""
		exit 1
	}
	return $grub2_path
}
