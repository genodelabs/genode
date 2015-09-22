/*
 * \brief  Test for PCI bus driver
 * \author Norman Feske
 * \date   2008-01-18
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>

using namespace Genode;

enum { INTEL_VENDOR_ID = 0x8086 };


/**
 * Print device information
 */
static void print_device_info(Platform::Device_capability device_cap)
{
	if (!device_cap.valid()) {
		PERR("Invalid device capability");
		return;
	}

	Platform::Device_client device(device_cap);

	unsigned char bus = 0, dev = 0, fun = 0;
	device.bus_address(&bus, &dev, &fun);
	unsigned short vendor_id = device.vendor_id();
	unsigned short device_id = device.device_id();
	unsigned      class_code = device.class_code() >> 8;

	printf("%02x:%02x.%x %04x: %04x:%04x (Vendor %s)\n",
	       bus, dev, fun, class_code, vendor_id, device_id,
	       vendor_id == INTEL_VENDOR_ID ? "Intel" : "unknown");

	for (int resource_id = 0; resource_id < 6; resource_id++) {

		Platform::Device::Resource resource = device.resource(resource_id);

		if (resource.type() != Platform::Device::Resource::INVALID)
			printf("  Resource %d (%s): base=0x%08x size=0x%08x %s\n", resource_id,
			       resource.type() == Platform::Device::Resource::IO ? "I/O" : "MEM",
			       resource.base(), resource.size(),
			       resource.prefetchable() ? "prefetchable" : "");
	}
}


int main(int argc, char **argv)
{
	printf("--- Platform test started ---\n");

	/* open session to pci service */
	static Platform::Connection pci;

	/*
	 * Iterate through all installed devices
	 * and print the available device information.
	 */
	Platform::Device_capability prev_device_cap,
	                            device_cap = pci.first_device();
	while (device_cap.valid()) {
		print_device_info(device_cap);

		pci.release_device(prev_device_cap);
		prev_device_cap = device_cap;
		device_cap = pci.next_device(prev_device_cap);
	}

	/* release last device */
	pci.release_device(prev_device_cap);

	printf("--- Platform test finished ---\n");

	return 0;
}
