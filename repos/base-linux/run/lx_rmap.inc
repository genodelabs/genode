#
# \brief  Test for Linux-specific region map
# \author Christian Helmuth
# \date   2013-09-06
#

assert {[have_spec linux]}

build  [list core init lib/ld [test_build_target]]

create_boot_directory

install_config {
config
+ parent-provides
  + service ROM
  + service PD
  + service LOG
  + service CPU

+ default-route | + any-service | + parent

+ start } [test_program_name] { | ram: 8M | caps: 100
-
}

build_boot_image [build_artifacts]

run_genode_until "child \"[test_program_name]\" exited with exit value 0.*\n" 10

puts "Test succeeded"

# vi: set ft=tcl :
