#
# \brief  Utilities for accessing depot content from run scripts
# \author Norman Feske
# \date   2017-03-29
#

#
# Return depot directory path
#
# \param --depot-dir   set depot directory to given path, otherwise
#                      default value [genode_dir]/depot is used
#
proc depot_dir { } {
	return [cmd_arg_first --depot-dir "[genode_dir]/depot"]
}


##
# Return spec value to be used to access binary archives
#
proc depot_spec { } {
	if {[have_spec x86_32]}  { return "x86_32" }
	if {[have_spec x86_64]}  { return "x86_64" }
	if {[have_spec arm_v6]}  { return "arm_v6" }
	if {[have_spec arm_v7a]} { return "arm_v7a" }
	if {[have_spec arm_v8a]} { return "arm_v8a" }
	if {[have_spec riscv]}   { return "riscv" }

	puts stderr "Error: unsupported architecture"
	exit 1
}


#
# Variable used for keeping track of archives that are missing from the
# depot. The list is populated by calls of 'import_from_depot' and evaluated
# at the boot-image-creation stage via 'check_for_missing_depot_archives'.
#
# Each list element is a list of <user>, <type>, <spec>, <name>, and <version>.
#
set _missing_depot_archives {}


#
# Pattern to parse a version-less archive path into <user>, <type>, <name>
#
proc _depot_archive_path_pattern { } {
	return {^([\w\d\-]+)/([\w]+)/([\w\d\-_]+)$} }


#
# Pattern to parse a versioned archive path into <user>, <type>, <name>, <version>
#
proc _depot_archive_versioned_path_pattern { } {
	return {^([\w\d\-]+)/([\w]+)/([\w\d\-_]+)/([\w\d\-\._]+)$} }


#
# Pattern to parse a binary archive path into <user>, <spec>, <name>.
#
proc _depot_bin_archive_path_pattern { } {
	return {^([\w\d\-]+)/bin/([\w\d]+)/([\w\d\-_]+)$} }


#
# Pattern to parse a versioned api name into <name>, <version>
#
proc _depot_api_versioned_name_pattern { } {
	return {^([\w\d\-_]+)/([\w\d\-\._]+)$} }


##
# Determine content of a pkg archive and its dependencies
#
proc _collect_pkg_archive_from_depot { user name version } {

	global _missing_depot_archives

	set archive_dir "$user/pkg/$name/$version"
	set archives_file "[depot_dir]/$archive_dir/archives"

	if {![file exists $archives_file]} {
		puts stderr "Error: missing file $archives_file"
		exit 1
	}

	set fh [open $archives_file "RDONLY"]
	set archives [read $fh]
	close $fh

	set content "$archive_dir"
	foreach archive $archives {
		if {[regexp [_depot_archive_versioned_path_pattern] $archive dummy user type name version]} {
			if {($type == "pkg") || ($type == "src") || ($type == "raw")} {
				set content [concat $content [_collect_from_depot $archive]]
			}
		}
	}

	return $content
}


proc _copy_directory_content_to_run_dir { dir } {

	if {![file isdirectory $dir]} {
		puts stderr "Error: expected directory at '$dir'"
		exit 1
	}

	foreach file [glob -nocomplain -directory $dir *] { copy_file $file [run_dir]/genode/ }
}


proc _collect_raw_archive_from_depot { user name version } {
	return "$user/raw/$name/$version" }


##
# Determine binary and api content for a given source archive
#
proc _collect_src_archive_from_depot { user name version } {

	global _missing_depot_archives;

	set src_archive_dir "$user/src/$name/$version"
	set bin_archive_dir "$user/bin/[depot_spec]/$name/$version"
	set used_apis_file  "[depot_dir]/$src_archive_dir/used_apis"

	set content {}

	if {[file exists [depot_dir]/$bin_archive_dir]} {
		append content [list $src_archive_dir $bin_archive_dir]
	} else {
		lappend _missing_depot_archives [list $user bin [depot_spec] $name $version]
	}

	if {![file exists $used_apis_file]} {
		puts stderr "Error: missing file $used_apis_file"
		exit 1
	}

	set fh [open $used_apis_file "RDONLY"]
	set used_apis [read $fh]
	close $fh

	foreach api $used_apis {
		#
		# The 'used_apis' file can contain fully specified entries or shortcuts.
		# A fully specified entry has the form '<user>/api/<name>/version'.
		# A shortcut has the form '<name>/version'. In this case, the '<user>' is
		# implicitely equal to the depot user of the src archive.
		#
		if {![regexp [_depot_archive_versioned_path_pattern] $api dummy user dummy api_name api_version]} {
			regexp [_depot_api_versioned_name_pattern] $api dummy api_name api_version }
		if {![_depot_contains_archive $user api $api_name $api_version]} {
			lappend _missing_depot_archives [list $user api "" $api_name $api_version]
			continue
		}
		lappend content $user/api/$api_name/$api_version
	}

	return $content
}


##
# Determine the current version for the given archive
#
# This function tries to determine the version information from the Genode
# source tree. It exits with an error if the archive is missing from the
# depot.
#
proc _current_depot_archive_version { type name } {

	set hash_rel_path "recipes/$type/$name/hash"
	set repo [repository_contains $hash_rel_path]

	if {$repo == ""} {
		puts stderr "Error: recipe for '$type/$name' not found - unable to guess version"
		exit 1
	}

	set fh [open "$repo/$hash_rel_path" "RDONLY"]
	set version [lindex [gets $fh] 0]
	close $fh

	return $version
}


proc _depot_contains_archive { user type name version } {

	return [file exists [depot_dir]/$user/$type/$name/$version]
}


proc _depot_auto_update { archives } {

	set archives_to_create { }
	foreach archive $archives {

		if {[regexp [_depot_archive_path_pattern] $archive dummy user type name]} {

			if {$type == "pkg"} {
				lappend archives_to_create "$user/pkg/[depot_spec]/$name" }

			if {$type == "src"} {
				lappend archives_to_create "$user/bin/[depot_spec]/$name" }

			if {$type == "raw"} {
				lappend archives_to_create "$user/raw/$name" }
		}
	}

	# remove duplicates
	set archives_to_create [lsort -unique $archives_to_create]

	if {[llength $archives_to_create] == 0} {
		return }

	set    cmd "[genode_dir]/tool/depot/create $archives_to_create "
	append cmd "CROSS_DEV_PREFIX=[cross_dev_prefix] "
	append cmd "DEPOT_DIR=[depot_dir] "
	append cmd "UPDATE_VERSIONS=1 FORCE=1 REBUILD= "

	if {[have_cmd_switch --ccache]} {
		append cmd "CCACHE=1 " }

	set make_j_arg ""
	regexp {.*(-j\d*)} [cmd_arg_first --make ""] dummy make_j_arg
	append cmd "$make_j_arg "

	puts "update depot: $cmd"

	if {[catch {exec {*}$cmd >@ stdout 2>@ stderr}]} {
		puts stderr "\nError: tool/depot/create during depot-auto-update failed\n"
		exit 1
	}
}


# keep track of recursion to perform '_depot_auto_update' only at the top level
set _collect_from_depot_nesting_level 0


proc _collect_from_depot { archives } {

	global _missing_depot_archives
	global _collect_from_depot_nesting_level

	incr _collect_from_depot_nesting_level

	# fill depot with up-to-date content if --depot-auto-update is enabled
	if {[have_cmd_switch --depot-auto-update]} {
		if {$_collect_from_depot_nesting_level == 1} {
			_depot_auto_update $archives } }

	set all_content {}

	foreach archive $archives {

		set version ""

		#
		# Try to parse versioned archive path. If no version is specified, use
		# the current version as present in the source tree.
		#
		if {![regexp [_depot_archive_versioned_path_pattern] $archive dummy user type name version]} {

			if {[regexp [_depot_archive_path_pattern] $archive dummy user type name]} {
				set version [_current_depot_archive_version $type $name]

			} else {
				puts stderr "Error: malformed depot-archive path '$archive',"
				puts stderr "       expected '<user>/<type>/<name>'"
				exit 1
			}
		}

		if {$version == ""} {
			puts stderr "Error: unable to guess version of '$type/$name' archive"
			exit 1
		}

		if {![_depot_contains_archive $user $type $name $version]} {
			lappend _missing_depot_archives [list $user $type "" $name $version]
			continue
		}

		set content {}

		switch $type {

			"pkg" { set content [_collect_pkg_archive_from_depot $user $name $version] }
			"src" { set content [_collect_src_archive_from_depot $user $name $version] }
			"raw" { set content [_collect_raw_archive_from_depot $user $name $version] }

			default {
				puts stderr "Error: unknown depot-archive type '$type'"
				exit 1
			}
		}
		set all_content [concat $all_content $content]
	}

	incr _collect_from_depot_nesting_level -1
	return $all_content
}


##
# Parse depot user from run tool arguments with a fallback to "genodelabs"
#
#
proc depot_user {} { return [cmd_arg --depot-user genodelabs] }


##
# Import binary and raw content of the specified depot archives into the run
# directory of the scenario
#
proc import_from_depot { args } {

	foreach subdir [_collect_from_depot [join $args " "]] {

		# prevent src, api, and pkg archives from inflating the boot image
		if {[regexp [_depot_archive_versioned_path_pattern] $subdir dummy user type]} {
			if {$type == "src"} continue;
			if {$type == "api"} continue;
			if {$type == "pkg"} continue;
		}

		_copy_directory_content_to_run_dir "[depot_dir]/$subdir"
	}
}


##
# Assemble tar archive from binary content of the depot
#
proc create_tar_from_depot_binaries { archive_path args } {

	# filter out api and src archives from requested depot content
	set content {}
	foreach subdir [_collect_from_depot $args] {
		if {[regexp [_depot_archive_versioned_path_pattern] $subdir dummy user type]} {
			if {$type == "src"} continue;
			if {$type == "api"} continue;
		}
		lappend content $subdir
	}

	check_for_missing_depot_archives

	if {[llength $args] > 0} {
		eval "exec tar cf $archive_path -C [depot_dir] [lsort -unique $content]"
	} else {
		eval "exec tar cf $archive_path -T /dev/null"
	}
}


##
# Append src and api packages to the tar archive
#
proc append_src_and_api_depot_packages_to_tar { archive_path args } {

	set content {}
	foreach subdir [_collect_from_depot $args] {
		if {[regexp [_depot_archive_versioned_path_pattern] $subdir dummy user type]} {
			if {($type != "src") && ($type != "api")} continue;
			lappend content $subdir
		}
	}

	check_for_missing_depot_archives

	eval "exec tar rf $archive_path -C [depot_dir] [lsort -unique $content]"
}


proc _locally_available_recipe { user type name version } {

	if {$type == "bin"} { set type "src" }

	if {[repository_contains "recipes/$type/$name/hash"] == ""} {
		return 0 }

	if {$version != [_current_depot_archive_version $type $name]} {
		return 0 }

	return 1
}


##
# Check for the completeness of the imported depot content
#
# This function aborts the run script if any archives are missing.
#
proc check_for_missing_depot_archives { } {

	global _missing_depot_archives

	if {[llength $_missing_depot_archives] == 0} { return }

	puts stderr "\nError: missing depot archives:"

	#
	# Try to assist the user with obtaining the missing archives
	#
	# For missing archives that belong to the configured depot user, the
	# user should be able to created them from the source tree as long as
	# recipe exists.
	#
	# Archives that do not belong to the configured depot user may be
	# downloaded.
	#
	# XXX Present this option only if the URL and public key of the
	#     archives user is known
	# XXX Present download option even for archives that can be created locally
	#

	set nonexisting_archives {}
	set local_user_archives {}
	set foreign_archives {}

	set _missing_depot_archives [lsort -unique $_missing_depot_archives]

	foreach archive $_missing_depot_archives {

		set user    [lindex $archive 0]
		set type    [lindex $archive 1]
		set spec    [lindex $archive 2]
		set name    [lindex $archive 3]
		set version [lindex $archive 4]

		#
		# If a pkg archive is missing, suggest to obtain the binary-pkg
		# archive (matching the build directory) immediately, which implies
		# the pkg archive. Otherwise, the user would first obtain the pkg
		# archive and its source dependencies, and then get an error for
		# the missing binary archives on the next attempt to execute the
		# run script.
		#
		if {$type == "pkg"} { set spec "[depot_spec]" }
		if {$type == "src"} {
			set type "bin"
			set spec "[depot_spec]"
		}

		set path "$user/$type/$name"
		if {$type == "bin" || $type == "pkg"} {
			set path "$user/$type/$spec/$name" }

		puts stderr "       $path/$version"

		if {[_locally_available_recipe $user $type $name $version]} {
			lappend local_user_archives $path
		} else {
			lappend foreign_archives $path/$version
		}
	}

	if {[llength $local_user_archives] || [llength $foreign_archives]} {
		puts stderr "" }

	if {[llength $local_user_archives]} {
		append create_args " CROSS_DEV_PREFIX=[cross_dev_prefix]"
		puts stderr "You may create the following archives locally by executing:\n"
		puts stderr "  [genode_dir]/tool/depot/create $local_user_archives$create_args\n"
	}

	if {[llength $foreign_archives]} {
		puts stderr "You may try to download the following archives by executing:\n"
		puts stderr "  [genode_dir]/tool/depot/download $foreign_archives\n"
	}

	exit 1
}


proc drivers_interactive_pkg { } {

	if {[have_board imx53_qsb_tz]} { return drivers_interactive-imx53_qsb }

	return "drivers_interactive-[board]"
}


proc drivers_nic_pkg { } { return "drivers_nic-[board]" }
