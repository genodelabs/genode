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

#include <os/slave.h>
#include <os/attached_rom_dataspace.h>

#include "pci_session_component.h"
#include "pci_device_config.h"

using namespace Genode;
using namespace Platform;

class Device_pd_policy : public Genode::Slave_policy
{
	private:

		Genode::Root_capability _cap;
		Genode::Lock _lock;

	protected:

		char const **_permitted_services() const
		{
			static char const *permitted_services[] = { "LOG", "CAP", "RM",
			                                            "CPU", 0 };
			return permitted_services;
		};

	public:

		Device_pd_policy(Genode::Rpc_entrypoint &slave_ep)
		:
			Slave_policy("device_pd", slave_ep),
			_lock(Genode::Lock::LOCKED)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			/* wait for 'platform_drv' to announce the DEVICE_PD service */
			if (Genode::strcmp(service_name, "DEVICE_PD"))
				return false;

			_cap = root;

			_lock.unlock();

			return true;
		}

		Genode::Root_capability root() {
			if (!_cap.valid())
				_lock.lock();
			return _cap;
		}
};


int main(int argc, char **argv)
{
	printf("platform driver started\n");

	/*
	 * Initialize server entry point
	 */
	enum {
		STACK_SIZE              = 2 * sizeof(addr_t)*1024,
		DEVICE_PD_RAM_QUOTA = 196 * 4096,
	};

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "platform_ep");

	/* use 'device_pd' as slave service */
	Session_capability session_dev_pd;
	Genode::Root_capability device_pd_root;
	try  {
		static Rpc_entrypoint   device_pd_ep(&cap, STACK_SIZE, "device_pd_slave");
		static Device_pd_policy device_pd_policy(device_pd_ep);
		static Genode::Slave    device_pd_slave(device_pd_ep, device_pd_policy,
		                                        DEVICE_PD_RAM_QUOTA);
		device_pd_root = device_pd_policy.root();
	} catch (...) {
		PWRN("PCI device protection domain for IOMMU support is not available");
	}

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
	static Platform::Root root(&ep, &sliced_heap, DEVICE_PD_RAM_QUOTA,
	                           device_pd_root, report_addr);

	env()->parent()->announce(ep.manage(&root));

	Genode::sleep_forever();
	return 0;
}
