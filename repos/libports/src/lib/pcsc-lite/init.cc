/*
 * \brief  pcsc-lite initialization
 * \author Christian Prochaska
 * \date   2016-10-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/xml_node.h>

/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

		unsigned int vendor_id = 0x0000;
		unsigned int product_id = 0x0000;

		int fd = open("/config.pcsc-lite", O_RDONLY);

		if (fd < 0) {
			Genode::error("Could not open 'config.pcsc-lite'");
			exit(1);
		}
		
		char config[128];
		if (read(fd, config, sizeof(config)) < 0) {
			Genode::error("Could not read 'config.pcsc-lite'");
			exit(1);
		}

		try {

			Genode::Xml_node config_node(config);

			vendor_id = config_node.attribute_value("vendor_id", 0U);
			product_id = config_node.attribute_value("product_id", 0U);

		} catch (...) {
			Genode::error("Error parsing 'config.pcsc-lite'");
			exit(1);
		}

		close(fd);

		char device[14];

		snprintf(device, sizeof(device), "usb:%04x/%04x", vendor_id, product_id);

		RFAllocateReaderSpace(0);
		(void)RFAddReader("CCID", 0, "/", device);
	}
};


extern "C" void initialize_pcsc_lite()
{
	static Pcsc_lite_initializer pcsc_lite_initializer;
}
