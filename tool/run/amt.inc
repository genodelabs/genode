##
# Check whether AMT support is available
#
proc is_amt_available { {host "" } {password "" } } {
	if {![have_spec x86]} { return false }

	#
	# Exit execution if parameter are not set rather returning
	# false because we cannot recover anyway.
	#
	if {[string compare $host ""] == 0} {
		puts "Aborting, AMT host not specified."
		exit -1
	}

	if {[string compare $password ""] == 0} {
		puts "Aborting,  AMT password not set."
		exit -1
	}

	if {[have_installed amtterm] &&
	    [expr [have_installed amttool] || [have_installed wsman] ] } {
		return true
	}

	puts "No support for Intel's AMT detected."
	return false
}
