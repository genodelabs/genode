/*
 * \brief  Platform driver for x86
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <cap_session/connection.h>

#include <os/attached_rom_dataspace.h>

#include "pci_session_component.h"
#include "pci_device_config.h"
#include "device_pd.h"

using namespace Genode;
using namespace Platform;

int main(int argc, char **argv)
{
	printf("platform driver started\n");

	/*
	 * Initialize server entry point
	 */
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "platform_ep");

	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());


	/**
	 * If we are running with ACPI support, wait for the first report_rom
	 */
	bool wait_for_acpi = true;
	char * report_addr = nullptr;

	try {
		char yesno[4];
		Genode::config()->xml_node().attribute("acpi").value(yesno, sizeof(yesno));
		wait_for_acpi = strcmp(yesno, "no");
	} catch (...) { }

	if (wait_for_acpi) {
		static Attached_rom_dataspace acpi_rom("acpi");

		Signal_receiver sig_rec;
		Signal_context  sig_ctx;
		Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

		acpi_rom.sigh(sig_cap);

		while (!acpi_rom.is_valid()) {
			sig_rec.wait_for_signal();
			acpi_rom.update();
		}
		report_addr = acpi_rom.local_addr<char>();
		sig_rec.dissolve(&sig_ctx);
	}

	/*
	 * Let the entry point serve the PCI root interface
	 */
	static Platform::Root root(&ep, &sliced_heap, report_addr, cap);

	env()->parent()->announce(ep.manage(&root));

	Genode::sleep_forever();
	return 0;
}
