source ${genode_dir}/repos/libports/run/qt5_drivers.inc

set feature(Input) 1
set feature(Framebuffer) 1
set feature(Timer) 1

#
# Build
#

proc qt5_build_components { feature_arg } {

	upvar $feature_arg feature

	set build_components {
		core
		init
	}

	append build_components [drivers_build_components feature]

	append build_components {
		server/nitpicker
	}

	return $build_components
}


#
# Configuration
#

proc qt5_parent_provides { feature_arg } {

	upvar $feature_arg feature

	set parent_provides [drivers_parent_provides feature]

	return $parent_provides
}

proc qt5_start_nodes { feature_arg } {

	upvar $feature_arg feature

	set start_nodes [drivers_start_nodes feature]

	append start_nodes {
		<start name="nitpicker">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Nitpicker"/></provides>
		</start>
	}

	return $start_nodes
}

#
# Boot modules
#

proc qt5_boot_modules { feature_arg } {

	upvar $feature_arg feature

	set boot_modules {
		core
		init
	}

	append boot_modules [drivers_boot_modules feature]

	append boot_modules {
		nitpicker
	}

	return $boot_modules
}

