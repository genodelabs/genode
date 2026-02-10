#
# This file includes snippets to generate the 'wifi_config' for the
# various wireless LAN driver for each test run-script.
#

proc wifi_ssid { } {
	return $::env(GENODE_WIFI_SSID)
}

proc wifi_psk { } {
	return $::env(GENODE_WIFI_PSK)
}

proc wifi_wpa { } {
	if {![info exists ::env(GENODE_WIFI_WPA)]} {
		return WPA2
	}
	return $::env(GENODE_WIFI_WPA)
}

proc wifi_verbose { } {
	if {![info exists ::env(GENODE_WIFI_VERBOSE)]} {
		return false
	}
	return true
}

proc wifi_debug { } {
	if {![info exists ::env(GENODE_WIFI_DEBUG)]} {
		return false
	}
	return true
}

#
# wifi-driver config generator (supporting a network list)
#
# You may script your tests with this function in the dynamic_rom config below.
# The syntax for the networks parameter is
#
#   { ssid protection passphrase explicit_scan }
#
# Example dynamic_rom config:
#
#   {<inline description="connect to two networks">
#   } [wifi_config 5 no [list "net1 WPA2 net1_psk no" "net2 WPA2 net2_psk no"]] {
#   </inline>
#   <inline description="connect to two netowrks, but net2 is hidden">
#   } [wifi_config 5 no [list "net1 WPA2 net1_psk no" "net2 WPA2 net2_psk yes"]] {
#   </inline>}
#

proc wifi_config { scan_interval update_quality_interval rfkill networks } {

	set    ident  "      |"
	set    config "$ident + wifi_config\n"
	append config "$ident   | verbose: [wifi_verbose]\n"
	if {[wifi_debug]} {
		append config "$ident   | log_level: debug\n"
	}
	append config "$ident   | scan_interval:           $scan_interval\n"
	append config "$ident   | update_quality_interval: $update_quality_interval\n"
	append config "$ident   | rfkill:                  $rfkill\n"
	foreach n $networks {
		if {[lindex $n 4] == "yes"} {
			append config "$ident   + explicit_scan | ssid: [lindex $n 0]\n"
		}
		append config "$ident   + network\n"
		append config "$ident     | ssid:       [lindex $n 0]\n"
		append config "$ident     | protection: [lindex $n 1]\n"
		append config "$ident     | passphrase: [lindex $n 2]\n"
	}

	return $config
}
