proc setup_shim_and_sign_grub2 { nickname target_dir } {

	set host_shim_path "/usr/lib/shim"
	set check_binaries "mmx64.efi shimx64.efi"
	set host_binaries  ""

	foreach binary $check_binaries {
		set filename "$host_shim_path/$binary"
		if {[file exists "$filename.signed"]} {
			lappend host_binaries $binary.signed
			continue
		}

		if {[file exists "$filename"]} {
			lappend host_binaries $binary
			continue
		}

		puts "Error: shim binary file $host_shim_path/$binary missing"
		puts "shim packages of your distribution are required"
		exit -1
	}

	foreach binary $host_binaries {
		catch {exec [installed_command sbverify] --list $host_shim_path/$binary} result

		puts "using $host_shim_path/$binary "
		puts $result

		if {[regexp "No signature table present" $result]} {
			puts "$binary has no signatures attached"
			exit -1
		}
	}

	exec cp $host_shim_path/[lindex $host_binaries 0] $target_dir/mmx64.efi
	exec cp $host_shim_path/[lindex $host_binaries 1] $target_dir/bootx64.efi

	puts "Export certificate for nickname '$nickname' to $target_dir/$nickname.cer"
	try {
		exec [installed_command sudo] [installed_command certutil] \
		       -d /etc/pki/pesign -n $nickname -Lr >$target_dir/$nickname.cer
	} on error { } {
		puts ""
		puts "Certificate with nickname '$nickname' not found!"
		puts ""
		puts "Notes for creating a certificate:"
		puts ""
		puts " sudo efikeygen --self-sign --common-name 'CN=YOUR COMPANY' --nickname '$nickname'"
		puts ""
		puts " Hint: newer efikeygen version may require --kernel"
		puts ""
		puts " The public and private keys are stored in the /etc/pki/pesign/ directory."
		puts " For more detailed information please consider documentation of efikeygen."
		puts ""

		exit -1
	}

	puts "Invoking 'pesign' for grub2 efi image"
	exec [installed_command sudo] [installed_command pesign] \
	     --in=[get_grub2_dir]/boot/grub2/grub2_64.efi \
	     --out=$target_dir/grubx64.efi \
	     -c $nickname --sign
}
