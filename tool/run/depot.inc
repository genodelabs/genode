#
# \brief  Utilities for accessing depot content from run scripts
# \author Norman Feske
# \date   2017-03-29
#

proc depot_dir { } { return [genode_dir]/depot }


##
# Return spec value to be used to access binary archives
#
proc depot_spec { } {
	if {[have_spec x86_32]} { return "x86_32" }
	if {[have_spec x86_64]} { return "x86_64" }
	if {[have_spec arm_v7]} { return "arm_v7" }
}


#
# Variable used for keeping track of archives that are missing from the
# depot. The list is populated by calls of 'import_from_depot' and evaluated
# at the boot-image-creation stage via 'check_for_missing_depot_archives'.
#
set _missing_depot_archives {}


#
# Pattern to parse an archive path into <user>, <type>, <name>
#
proc _depot_archive_path_pattern { } { return {^([\w\d]+)/([\w]+)/([\w\d\-_]+)$} }

#
# Pattern to parse an binary archive path into <user>, <spec>, <name>.
#
proc _depot_bin_archive_path_pattern { } { return {^([\w\d]+)/bin/([\w\d]+)/([\w\d\-_]+)$} }
proc _depot_lib_archive_path_pattern { } { return {^([\w\d]+)/bin/([\w\d]+)/[\w\d\-_]+/([\w\d\-_]+)$} }


##
# Import a pkg archive and its dependencies
#
proc _import_pkg_archive_from_depot { user name } {

	global _missing_depot_archives

	set archive_dir "[depot_dir]/$user/pkg/$name"

	if {![file exists $archive_dir/archives]} {
		puts "Error: missing file $archive_dir/archives"
		exit 1
	}

	set fh [open "$archive_dir/archives" "RDONLY"]
	set archives [read $fh]
	close $fh

	foreach archive $archives {
		if {[regexp [_depot_archive_path_pattern] $archive dummy user type name]} {
			if {($type == "pkg") || ($type == "src") || ($type == "raw")} {
				import_from_depot $archive
			}
		}
	}
}


proc _copy_directory_content_to_run_dir { dir } {

	if {![file isdirectory $dir]} {
		puts stderr "Error: expected directory at '$dir'"
		exit 1
	}

	foreach file [glob -directory $dir *] { file copy -force $file [run_dir]/genode/ }
}


proc _import_raw_archive_from_depot { user name } {

	_copy_directory_content_to_run_dir "[depot_dir]/$user/raw/$name"
}


##
# Copy the binary content for a given source archive into the run directory
#
proc _import_src_archive_from_depot { user name } {

	global _missing_depot_archives;

	set bin_archive "$user/bin/[depot_spec]/$name"

	#
	# If the binary archive contains a library, direct 'bin_archive' to the
	# correct API sub directory.
	#
	set api_file_path "[depot_dir]/$user/src/$name/api"
	if {[file exists $api_file_path]} {
		set fh [open "$api_file_path" "RDONLY"]
		set api [read $fh]
		close $fh
		regsub -all {\s} $api {} api
		if {$api != ""} {
			set bin_archive "$user/bin/[depot_spec]/$api/$name" }
	}

	if {[file exists [depot_dir]/$bin_archive]} {
		_copy_directory_content_to_run_dir "[depot_dir]/$bin_archive"
	} else {
		lappend _missing_depot_archives $bin_archive
	}
}


##
# Determine the version-suffixed name of an archive
#
# This function return an empty string if the archive is missing from the
# depot.
#
proc _versioned_depot_archive_name { user type name } {

	# if correctly versioned archive is specified, use it
	if {[file exists [depot_dir]/$user/$type/$name]} { return $name }

	#
	# The given archive name may lack the version identifier if it refers
	# to an archive generated locally from the Genode repository. In this case,
	# we try to determine the version information from the Genode source tree.
	#
	set hash_rel_path "recipes/$type/$name/hash"
	set repo [repository_contains $hash_rel_path]

	if {$repo != ""} {
		set fh [open "$repo/$hash_rel_path" "RDONLY"]
		set version [lindex [gets $fh] 0]
		close $fh

		append name "-" $version
		if {[file exists [depot_dir]/$user/$type/$name]} {
			return $name }
	}

	return ""
}


proc import_from_depot { args } {

	global _missing_depot_archives

	foreach archive $args {

		if {[regexp [_depot_archive_path_pattern] $archive dummy user type name]} {

			set versioned_name [_versioned_depot_archive_name $user $type $name]
			if {$versioned_name == ""} {
				lappend _missing_depot_archives $archive

			} else {

				switch $type {

					"pkg" { _import_pkg_archive_from_depot $user $versioned_name }
					"src" { _import_src_archive_from_depot $user $versioned_name }
					"raw" { _import_raw_archive_from_depot $user $versioned_name }

					default {
						puts stderr "Error: unknown depot-archive type '$type'"
						exit 1
					}
				}
			}
		} else {
			puts stderr "Error: malformed depot-archive path '$archive',"
			puts stderr "       expected '<user>/<type>/<name>'"
			exit 1
		}
	}
}


proc _locally_available_recipe { user type name } {

	if {[repository_contains "recipes/$type/$name/hash"] != ""} {
		return $name }

	#
	# If the supplied 'name' is a versioned name (as obtained from a pkg
	# 'archives' filed already stored in the depot, we try to find a recipe
	# in the source tree that matches the specified name and version. If
	# we find the matching recipe, we can build the archive locally using
	# the version-less recipe name.
	#
	set version_suffix_pattern {\-[^\-/]*$}
	set versioned_name $name

	while {[regexp -- $version_suffix_pattern $name dummy]} {

		# strip last part of version suffix
		regsub -- $version_suffix_pattern $name "" name

		if {[repository_contains "recipes/$type/$name/hash"] != ""} {
			if {[_versioned_depot_archive_name $user $type $name] == $versioned_name} {
				return $name } }
	}
	return ""
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

	foreach archive $_missing_depot_archives {
		puts stderr "       $archive"
		if {[regexp [_depot_archive_path_pattern] $archive dummy user type name]} {

			#
			# If a pkg archive is missing, suggest to obtain the binary-pkg
			# archive (matching the build directory) immediately, which implies
			# the pkg archive. Otherwise, the user would first obtain the pkg
			# archive and its source dependencies, and then get an error for
			# the missing binary archives on the next attempt to execute the
			# run script.
			#
			if {$type == "pkg"} { set archive "$user/pkg/[depot_spec]/$name" }
			if {$type == "src"} { set archive "$user/bin/[depot_spec]/$name" }

			if {[_locally_available_recipe $user $type $name] != ""} {
				lappend local_user_archives $archive
			} else {
				lappend foreign_archives $archive
			}
		} elseif {[regexp [_depot_lib_archive_path_pattern] $archive dummy user spec name]
		       || [regexp [_depot_bin_archive_path_pattern] $archive dummy user spec name]} {

			# source code is present in the current source tree
			set recipe [_locally_available_recipe $user src $name]
			if {$recipe != ""} {
				lappend local_user_archives $user/bin/$spec/$recipe

			# source code is present in the depot
			} elseif {[file isdirectory [depot_dir]/$user/src/$name]} {
				lappend local_user_archives $user/bin/$spec/$name

			} else {
				lappend foreign_archives $archive
			}
		}
	}

	append create_args " CROSS_DEV_PREFIX=[cross_dev_prefix]"

	if {[llength $local_user_archives]} {
		puts stderr "You may create the following archives locally:\n"
		puts stderr "  [genode_dir]/tool/depot/create $local_user_archives$create_args\n"
	}

	if {[llength $foreign_archives]} {
		puts stderr "You may try to download the following archives:\n"
		puts stderr "  [genode_dir]/tool/depot/download $foreign_archives\n"
	}

	exit 1
}


proc drivers_interactive_pkg { } {

	if {[have_spec linux]} { return drivers_interactive-linux }
	if {[have_spec x86]}   { return drivers_interactive-pc }
	if {[have_spec pbxa9]} { return drivers_interactive-pbxa9 }

	puts stderr "drivers_interactive package undefined for this build configuration"
	exit 1
}
