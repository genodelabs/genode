/*
 * \brief  VirtIO PCI NIC driver
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <platform_session/connection.h>
#include <virtio/device.h>

#include "component.h"

namespace Virtio_pci_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_pci_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Genode::Env            &env;
	Genode::Heap            heap{ env.ram(), env.rm() };
	Platform::Connection    pci { env };
	Platform::Device_client platform_device;
	Virtio::Device          virtio_device { env, platform_device };
	Virtio_nic::Root        root { env, heap, virtio_device, platform_device.irq(0) };

	Platform::Device_capability find_platform_device()
	{
		Platform::Device_capability device_cap;
		pci.with_upgrade([&] () { device_cap = pci.first_device(); });

		if (!device_cap.valid()) throw Device_not_found();

		return device_cap;
	}

	Main(Env &env)
	try : env(env), platform_device(find_platform_device())
	{
		log("--- VirtIO PCI driver started ---");
		env.parent().announce(env.ep().manage(root));
	}
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_pci_nic::Main main(env);
}
