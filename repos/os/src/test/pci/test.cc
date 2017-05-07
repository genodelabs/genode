/*
 * \brief  Test for PCI bus driver
 * \author Norman Feske
 * \date   2008-01-18
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/retry.h>

using namespace Genode;

enum { INTEL_VENDOR_ID = 0x8086 };


/**
 * Print device information
 */
static void print_device_info(Platform::Device_capability device_cap)
{
	if (!device_cap.valid()) {
		error("invalid device capability");
		return;
	}

	Platform::Device_client device(device_cap);

	unsigned char bus = 0, dev = 0, fun = 0;
	device.bus_address(&bus, &dev, &fun);
	unsigned short vendor_id = device.vendor_id();
	unsigned short device_id = device.device_id();
	unsigned      class_code = device.class_code() >> 8;

	log(Hex(bus, Hex::OMIT_PREFIX), ":",
	    Hex(dev, Hex::OMIT_PREFIX), ".",
	    Hex(fun, Hex::OMIT_PREFIX), " "
	    "class=", Hex(class_code), " "
	    "vendor=", Hex(vendor_id), " ",
	               (vendor_id == INTEL_VENDOR_ID ? "(Intel)" : "(unknown)"),
	    " device=", Hex(device_id));

	for (int resource_id = 0; resource_id < 6; resource_id++) {

		typedef Platform::Device::Resource Resource;

		Resource const resource = device.resource(resource_id);

		if (resource.type() != Resource::INVALID)
			log("  Resource ", resource_id, " "
			    "(", (resource.type() == Resource::IO ? "I/O" : "MEM"), "): "
				"base=", Genode::Hex(resource.base()), " "
				"size=", Genode::Hex(resource.size()), " ",
			    (resource.prefetchable() ? "prefetchable" : ""));
	}
}


void Component::construct(Genode::Env &env)
{
	log("--- Platform test started ---");

	/* open session to pci service */
	static Platform::Connection pci(env);

	Platform::Device_capability prev_device_cap, device_cap;

	pci.with_upgrade([&] () { device_cap = pci.first_device(); });

	/*
	 * Iterate through all installed devices
	 * and print the available device information.
	 */
	while (device_cap.valid()) {
		print_device_info(device_cap);

		pci.release_device(prev_device_cap);
		prev_device_cap = device_cap;

		pci.with_upgrade([&] () { device_cap = pci.next_device(device_cap); });
	}

	/* release last device */
	pci.release_device(prev_device_cap);

	log("--- Platform test finished ---");
}
