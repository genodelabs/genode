/*
 * \brief  pcsc-lite initialization
 * \author Christian Prochaska
 * \date   2016-10-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/xml_node.h>

/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* libusb includes */
#include <libusb.h>

/* pcsc-lite includes */
extern "C" {
#include <debuglog.h>
#include <readerfactory.h>
}

static constexpr bool verbose = false;

struct Pcsc_lite_initializer
{
	Pcsc_lite_initializer()
	{
		if (verbose) {
			/* pcscd -f */
			DebugLogSetLogType(DEBUGLOG_STDOUT_DEBUG);
			/* pcscd -d */
			DebugLogSetLevel(PCSC_LOG_DEBUG);
			/* pcscd -a */
			(void)DebugLogSetCategory(DEBUG_CATEGORY_APDU);
		}

		/*
		 * Find out vendor id and product id of the connected USB device
		 * and add it as reader.
		 */

		libusb_context *ctx = nullptr;

		libusb_init(&ctx);

		libusb_device **devs = nullptr;

		if (libusb_get_device_list(ctx, &devs) < 1) {
			Genode::error("Could not find a USB device.");
			exit(1);
		}

		struct libusb_device_descriptor device_descriptor;

		if (libusb_get_device_descriptor(devs[0], &device_descriptor) < 0) {
			Genode::error("Could not read the device descriptor of the "
			              "USB device.");
			exit(1);
		}

		libusb_free_device_list(devs, 1);

		libusb_exit(ctx);

		char device[14];

		snprintf(device, sizeof(device), "usb:%04x/%04x",
		         device_descriptor.idVendor, device_descriptor.idProduct);

		RFAllocateReaderSpace(0);
		(void)RFAddReader("CCID", 0, "/", device);
	}
};


extern "C" void initialize_pcsc_lite()
{
	static Pcsc_lite_initializer pcsc_lite_initializer;
}
