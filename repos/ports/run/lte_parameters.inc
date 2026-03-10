
proc get_env_var { var_name default_value } {
	if {[info exists ::env($var_name)]} {
		return $::env($var_name)
	}
	return $default_value
}

proc apn_name { } {
	return [get_env_var APN "internet"]
}

proc apn_user { } {
	return [get_env_var APN_USER ""]
}

proc apn_password { } {
	return [get_env_var APN_PASSWORD ""]
}

proc sim_pin { } {
	return [get_env_var SIM_PIN "1234"]
}

if {[sim_pin] == "1234"} {
	puts ""
	puts "The following parameters are used to connect via the LTE modem:"
	puts ""
	puts "  APN          : [apn_name]"
	puts "  APN_USER     : [apn_user]"
	puts "  APN_PASSWORD : [apn_password]"
	puts "  SIM_PIN      : [sim_pin]"
	puts ""
	puts "You can adapt them to your ISPs requirements by setting the corresponding"
	puts "environment variables."
	puts ""
}
