#!/usr/bin/expect

if {[have_cmd_switch --autopilot]} {
	set current_date [clock format [clock seconds] -format %a]
	assert {$current_date == "Sat" || $current_date == "Sun"} \
		"Autopilot mode is not supported on this platform today."
	}
}

run_genode_until $match_boot_string 40

set t_vm_boot_start    [clock seconds]
run_genode_until "$match_guest_log.*git clone" 90 [output_spawn_id]
set t_vm_git_clone     [clock seconds]
run_genode_until "$match_guest_log.*prepare ports" 70 [output_spawn_id]
set t_vm_prepare_ports [clock seconds]
run_genode_until "$match_guest_log.*make -j" 200 [output_spawn_id]
set t_vm_make_j        [clock seconds]
run_genode_until "$match_guest_log.*make done" 500 [output_spawn_id]
set t_vm_make_done     [clock seconds]

run_genode_until "$match_guest_log.*cpus=" 20 [output_spawn_id]
run_genode_until "$match_guest_log.*Done stress.sh" 10 [output_spawn_id]

set t_clone         [expr $t_vm_prepare_ports - $t_vm_git_clone]
set t_prepare_ports [expr $t_vm_make_j        - $t_vm_prepare_ports]
set t_make          [expr $t_vm_make_done     - $t_vm_make_j]

set results_end     [llength $results_expected]
set result_make     [lindex $results_expected [expr $results_end - 1]]
set result_prepare  [lindex $results_expected [expr $results_end - 2]]
set result_clone    [lindex $results_expected [expr $results_end - 3]]

set output_result "(host-tcl)             cpus=$use_vcpus - clone=$t_clone - prepare_port=$t_prepare_ports - make=$t_make"
set output_result_ci "(host)                 cpus=$use_vcpus - clone=$t_clone,$result_clone - prepare_port=$t_prepare_ports,$result_prepare - make=$t_make,$result_make"

set output_extra "boottimeVM=[expr $t_vm_git_clone - $t_vm_boot_start]"

puts "\n$output_result_ci - $output_extra - $vmm_name\n"
append output_host "$output_result\n"
append output_host_extra "$output_extra\n"


##
# Evaluate results of stress test
#
proc evaluate_stress { } {

	global output_host_extra
	global output_host
	global output
	global results_expected
	global boottime_expected

	set results_boottime [regexp -all -inline {[0-9]+} $output_host_extra]
	set results_host     [regexp -all -inline {[0-9]+} $output_host]
	set results_guest    [regexp -all -inline {[0-9]+} $output]
	set results_len      [llength $results_host]

	puts "host     results $results_host"
	puts "guest    results $results_guest"
	puts "expected results $results_expected"
	puts "boottime host    $results_boottime"
	puts "boottime expect  $boottime_expected"

	#
	# Sanity check
	#
	if {$results_len != [llength $results_expected] ||
		$results_len != [llength $results_guest] ||
		[llength $boottime_expected] != [llength $results_boottime]} {
		puts "Result length not as expected"
		exit -1
	}

	set result_miss 0

	for { set i 0 } { $i < $results_len } { incr i} {
		set res_h [lindex $results_host $i]
		set res_g [lindex $results_guest $i]
		set res_e [lindex $results_expected $i]
		if {$res_h != $res_e || $res_g != $res_e} {
			set diff_abs_h [expr abs($res_h - $res_e)]
			set diff_abs_g [expr abs($res_g - $res_e)]

			switch [expr $i % 4] {
				0 {
					# the cpu number count must be identical
					incr result_miss
				}
				1 {
					# give clone numbers 5s diff
					if { $diff_abs_h > 5 || $diff_abs_g > 5 } { incr result_miss }
				}
				2 {
					# give prepare ports 10s diff
					if { $diff_abs_h > 10 || $diff_abs_g > 10 } { incr result_miss }
				}
				3 {
					if { $diff_abs_h > 1 || $diff_abs_g > 1 } { incr result_miss }
				}
			}
		}
	}

	for { set i 0} { $i < $results_len } { incr i} {
		set res_h [lindex $results_boottime $i]
		set res_e [lindex $boottime_expected $i]

		if {$res_h != $res_e} {
			set diff_abs [expr abs($res_h - $res_e)]

			if { $diff_abs > 1 } { incr result_miss }
		}
	}

	if {$result_miss} {
		puts "$result_miss results deviate too much"
	}
}
