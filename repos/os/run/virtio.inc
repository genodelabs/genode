proc append_virtio_bus_build_components {} {
	global build_components
	if {[have_spec pci]} {
		append build_components { drivers/virtio_pci }
	} else {
		append build_components { drivers/virtio_mmio }
	}
}


proc append_virtio_bus_boot_modules {} {
	global boot_modules
	if {[have_spec pci]} {
		append boot_modules { virtio_pci }
	} else {
		append boot_modules { virtio_mmio fdt.lib.so }
	}
}


proc virtio_bus_binary { } {
	if {[have_spec pci]} { return virtio_pci }
	return virtio_mmio
}


proc virtio_bus_routes { } {
	set bus_routes ""

	if {[have_spec virt_qemu]} {
		append bus_routes {
			<service name="ROM" label="platform.dtb">
				<child name="platform_drv" />
			</service>}
	}

	return $bus_routes

}


proc virtio_bus_policy { } {
	return { }
}


proc virtio_bus_config {} {
	set bus_config ""

	append bus_config {
		<start name="virtio_bus">
			<binary name="} [virtio_bus_binary] {"/>
			<resource name="RAM" quantum="2M"/>
			<provides> <service name="VirtIO"/> </provides>
			<route>}

	append bus_config [virtio_bus_routes]

	append bus_config {
				<any-service><parent/><any-child/></any-service>
			</route>
			<config>}

	append bus_config [virtio_bus_policy]

	append bus_config {
			</config>
		</start>}
}


proc append_virtio_bus_config {} {
	global config
	append config [virtio_bus_config]
	return $config
}
