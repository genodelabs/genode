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

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <platform_session/connection.h>
#include <virtio/pci_device.h>

/* local includes */
#include "component.h"

namespace Virtio_pci_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_pci_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Genode::Env             & env;
	Genode::Heap              heap            { env.ram(), env.rm()   };
	Platform::Connection      pci             { env                   };
	Virtio::Device            virtio_device   { env, pci              };
	Attached_rom_dataspace    config_rom      { env, "config"         };
	Virtio_nic::Uplink_client uplink_client   { env, heap, virtio_device,
	                                            pci, config_rom.xml() };

	Main(Env &env) : env(env) {
		log("--- VirtIO PCI driver started ---"); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_pci_nic::Main main(env);
}
