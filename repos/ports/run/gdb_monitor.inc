#
# \brief  GDB command line arguments for setting a breakpoint in the 'main()' function 
# \author Christian Prochaska
# \date   2013-09-04
#

proc gdb_initial_breakpoint_cmds { target_binary_name } {

	#
	# We set a break in the 'binary_ready_hook_for_gdb()' function in the
	# dynamic linker and load the symbols of the application by using the
	# following gdb command sequence.
	#

	set gdb_cmds ""

	# don't ask for y/n when loading a new symbol file
	append gdb_cmds {-ex "set interactive-mode off" }

	# set a breakpoint in the 'binary_ready_hook_for_gdb' function
	append gdb_cmds {-ex "b binary_ready_hook_for_gdb" }

	# continue execution until the breakpoint triggers
	append gdb_cmds {-ex "c" }

	# delete the 'binary_ready_hook_for_gdb' breakpoint
	append gdb_cmds {-ex "delete 1" }

	# load the symbols of the test application
	append gdb_cmds "-ex \"file debug/$target_binary_name\" "

	# set search path for "sharedlibrary" to debug
	append gdb_cmds {-ex "set solib-search-path debug" }

	# load the symbols of loaded shared libraries
	append gdb_cmds {-ex "sharedlibrary" }

	return $gdb_cmds
}
