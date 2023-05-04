#
# \brief  Test for Linux-specific region map
# \author Christian Helmuth
# \date   2013-09-06
#

assert_spec linux

build  [list core init lib/ld [test_build_target]]

create_boot_directory

install_config {
<config>
	<parent-provides>
		<service name="ROM"/>
		<service name="PD"/>
		<service name="LOG"/>
		<service name="CPU"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>
	<start name="} [test_program_name] {">
		<resource name="RAM" quantum="8M"/>
	</start>
</config>}


build_boot_image [build_artifacts]

run_genode_until "child \"[test_program_name]\" exited with exit value 0.*\n" 10

puts "Test succeeded"

# vi: set ft=tcl :
