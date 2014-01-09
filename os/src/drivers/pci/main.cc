/*
 * \brief  PCI-bus driver
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

#include "pci_session_component.h"
#include "pci_device_config.h"

using namespace Genode;
using namespace Pci;

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
			Slave_policy("pci_device_pd", slave_ep),
			_lock(Genode::Lock::LOCKED)
		{ }

		bool announce_service(const char             *service_name,
		                      Genode::Root_capability root,
		                      Genode::Allocator      *alloc,
		                      Genode::Server         *server)
		{
			/* wait for 'pci_drv' to announce the PCI service */
			if (Genode::strcmp(service_name, "PCI_DEV_PD"))
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
	printf("PCI driver started\n");

	/*
	 * Initialize server entry point
	 */
	enum {
		STACK_SIZE              = 2 * sizeof(addr_t)*1024,
		PCI_DEVICE_PD_RAM_QUOTA = 196 * 4096,
	};

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "pci_ep");

	/* use 'pci_device_pd' as slave service */
	Session_capability session_dev_pd;
	try  {
		static Rpc_entrypoint   device_pd_ep(&cap, STACK_SIZE, "device_pd_slave");
		static Device_pd_policy device_pd_policy(device_pd_ep);
		static Genode::Slave    device_pd_slave(device_pd_ep, device_pd_policy,
		                                        PCI_DEVICE_PD_RAM_QUOTA);
		session_dev_pd = Genode::Root_client(device_pd_policy.root()).session("", Affinity());
	} catch (...) {
		PWRN("PCI device protection domain for IOMMU support is not available");
	}

	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/*
	 * Let the entry point serve the PCI root interface
	 */
	static Pci::Root root(&ep, &sliced_heap, PCI_DEVICE_PD_RAM_QUOTA,
	                      Genode::reinterpret_cap_cast<Device_pd>(session_dev_pd));
	env()->parent()->announce(ep.manage(&root));

	sleep_forever();
	return 0;
}
