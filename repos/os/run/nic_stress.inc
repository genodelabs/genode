append build_components {
	core init timer
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

append config {

<config>
	<parent-provides>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>}

append_if [string equal $type "nic_router"] config {

	<start name="nic_router" caps="1000">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Nic"/></provides>
		<config>
			<policy label_prefix="nic_stress_2" domain="default"/>
			<policy label_prefix="nic_stress_1" domain="default"/>
			<domain name="default" interface="10.0.2.55/24"/>
		</config>
	</start>

	<alias name="nic_server" child="nic_router"/>}

append_if [string equal $type "nic_bridge"] config {

	<start name="nic_router" caps="1000">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Nic"/></provides>
		<config>
			<policy label_prefix="nic_bridge" domain="default"/>
			<domain name="default" interface="10.0.2.55/24"/>
		</config>
	</start>

	<start name="nic_bridge" caps="1000">
		<resource name="RAM" quantum="50M"/>
		<provides><service name="Nic"/></provides>
		<config mac="02:02:02:02:42:00">
			<policy label_prefix="nic_stress_2"/>
			<policy label_prefix="nic_stress_1"/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_router"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<alias name="nic_server" child="nic_bridge"/>}

append config {

	<start name="nic_stress_1" caps="1000">
		<binary name="test-nic_stress"/>
		<resource name="RAM" quantum="50M"/>
		<config exit_support="} [exit_support] {">
			<construct_destruct nr_of_rounds="}   [nr_of_rounds   1] {"
			                    nr_of_sessions="} [nr_of_sessions 1] {"/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_server"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic_stress_2" caps="1000">
		<binary name="test-nic_stress"/>
		<resource name="RAM" quantum="100M"/>
		<config exit_support="} [exit_support] {">
			<construct_destruct nr_of_rounds="}   [nr_of_rounds 2] {"
			                    nr_of_sessions="} [nr_of_rounds 2] {"/>
		</config>
		<route>
			<service name="Nic"> <child name="nic_server"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

</config>}

install_config $config

append boot_modules {
	core init timer
	nic_router
	test-nic_stress
	ld.lib.so
}

append_if [string equal $type "nic_bridge"] boot_modules { nic_bridge }

build_boot_image $boot_modules

append qemu_args " -nographic "

run_genode_until [done_string] 300
