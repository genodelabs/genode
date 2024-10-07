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

set wifi_verbose false

proc wifi_config { scan_interval update_quality_interval rfkill networks } {
	global wifi_verbose

	set    config "<wifi_config"
	append config " verbose=\"[wifi_verbose]\""
	append config " scan_interval=\"$scan_interval\""
	append config " update_quality_interval=\"$update_quality_interval\""
	append config " rfkill=\"$rfkill\""
	append config ">\n"
	foreach n $networks {
		if {[lindex $n 4] == "yes"} {
			append config " <explicit_scan ssid=\"[lindex $n 0]\"/>\n"
		}
		append config "  <network"
		append config " ssid=\"[lindex $n 0]\""
		append config " protection=\"[lindex $n 1]\""
		append config " passphrase=\"[lindex $n 2]\""
		append config "/>\n"
	}
	append config "</wifi_config>\n"

	return $config
}

