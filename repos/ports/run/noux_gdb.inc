#
# Utility functions for run scripts using Noux GDB
#


#
# Return the name of the Noux GDB package for the configured platform
#
proc noux_gdb_pkg_name { } {
	if {[have_spec arm]} {
		return "gdb_arm"
	} elseif {[have_spec x86]} {
		return "gdb_x86"
	}
}


#
# Create a tar archive for a Noux application and its shared libraries (unstripped)
#
proc create_binary_tar { application_name application_binaries } {
	foreach application_binary ${application_binaries} { 
		exec tar ufv bin/${application_name}.tar -h -C debug ${application_binary}
	}
}


#
# Create a tar archive for the source code of a Noux application and its shared
# libraries.
#
# Currently, directories need to have their own tar records 
#
proc create_source_tar { application_name application_binaries } {
	exec mkdir -p bin/${application_name}-src
	foreach application_binary $application_binaries {
		set binary debug/[kernel_specific_binary ${application_binary} silent]
		puts "archive sources of $binary"
		set source_files [ exec [cross_dev_prefix]objdump -dl $binary | grep "^/.*:.*" | sed -e "s/:.*//" | uniq ]
		foreach source_file ${source_files} {
			# resolve '..' to avoid problems with 'tar' with parts like '/a/b/../'
			# where '/a' exists, but '/a/b' does not
			set source_file [file normalize ${source_file}]
			if [file exists ${source_file}] {
				set dirname [ exec dirname ${source_file}]
				exec mkdir -p bin/${application_name}-src${dirname}
				exec ln -sf ${source_file} bin/${application_name}-src${source_file}
			}
		}
	}
	exec tar chf bin/${application_name}-src.tar -C bin/${application_name}-src .
}


#
# Create tar archives for binaries and source code of a Noux application
#
proc create_binary_and_source_tars { application_name application_binaries } {
	create_binary_tar ${application_name} ${application_binaries}
	create_source_tar ${application_name} ${application_binaries}
}

