append build_components {
	core init timer lib/ld
	server/nic_router
	test/nic_stress
}

append_if [string equal $type "nic_bridge"] build_components { server/nic_bridge }

proc exit_support {} {
	if {[have_spec fiasco]} {
		return "no"
	}
	return "yes"
}

proc done_string {} {
	set done_string ""
	if {[have_spec fiasco]} {
		append done_string {.*?finished NIC stress test}
		append done_string {.*?\n}
		append done_string {.*?finished NIC stress test}
		append done_string {.*?\n}
	} else {
		append done_string {.*?"nic_stress_." exited with exit value 0}
		append done_string {.*?\n}
		append done_string {.*?"nic_stress_." exited with exit value 0}
		append done_string {.*?\n}
	}
	return $done_string
}

proc nr_of_rounds { test_id } {
	if {[have_spec sel4]} {
		switch $test_id {
			1 { return 9 }
			2 { return 7 }
		}
	} else {
		switch $test_id {
			1 { return 22 }
			2 { return 16 }
		}
	}
	return 0
}

proc nr_of_sessions { test_id } {
	switch $test_id {
		1 { return 11 }
		2 { return 17 }
	}
	return 0
}

build $build_components

create_boot_directory


proc nic_router_start_nodes { } { return {
+ start nic_router | caps: 1000 | ram: 10M
  + provides
    + service Nic
    + service Uplink
  + config
    + policy | label_prefix: nic_stress_2 | domain: default
    + policy | label_prefix: nic_stress_1 | domain: default
    + domain default | interface: 10.0.2.55/24

+ alias nic_server | child: nic_router
} }


proc nic_bridge_start_nodes { } { return {
+ start nic_router | caps: 1000 | ram: 10M
  + provides
    + service Nic
    + service Uplink
  + config
    + policy | label_prefix: nic_bridge | domain: default
    + domain default | interface: 10.0.2.55/24

+ start nic_bridge | caps: 1000 | ram: 50M
  + provides | + service Nic
  + config | mac: 02:02:02:02:42:00
    + policy | label_prefix: nic_stress_2
    + policy | label_prefix: nic_stress_1
  + route
    + service Nic | + child nic_router
    + any-service
      + parent
      + any-child

+ alias nic_server | child: nic_bridge
} }


proc nic_server_start_nodes { type } {
	if { $type == "nic_router" } { return [nic_router_start_nodes] }
	if { $type == "nic_bridge" } { return [nic_bridge_start_nodes] }
}


install_config {
config
+ parent-provides
  + service ROM
  + service IRQ
  + service IO_MEM
  + service IO_PORT
  + service PD
  + service RM
  + service CPU
  + service LOG

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 100 | ram: 1M

+ start timer
  + provides | + service Timer

} [nic_server_start_nodes $type] {

+ start nic_stress_1 | caps: 1000 | ram: 50M
  + binary test-nic_stress
  + config | exit_support: } [exit_support] {
  | + construct_destruct
  |                      nr_of_rounds:   } [nr_of_rounds   1] {
  |                      nr_of_sessions: } [nr_of_sessions 1] {
  + route
    + service Nic | + child nic_server
    + any-service
      + parent
      + any-child

+ start nic_stress_2 | caps: 1000 | ram: 100M
  + binary test-nic_stress
  + config | exit_support: } [exit_support] {
  | + construct_destruct
  |                      nr_of_rounds:   } [nr_of_rounds 2] {
  |                      nr_of_sessions: } [nr_of_rounds 2] {
  + route
    + service Nic | + child nic_server
    + any-service
      + parent
      + any-child
-
}

build_boot_image [build_artifacts]

append qemu_args " -nographic "

run_genode_until [done_string] 300
