/*
 * \brief  VirtIO MMIO NIC driver
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
#include <virtio/mmio_device.h>

/* local includes */
#include "component.h"

namespace Virtio_mmio_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_mmio_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Env                     & env;
	Heap                      heap            { env.ram(), env.rm() };
	Platform::Connection      platform        { env                 };
	Platform::Device          platform_device { platform,
	                                            Platform::Device::Type { "nic" } };
	Virtio::Device            device          { platform_device     };
	Attached_rom_dataspace    config_rom      { env, "config"       };
	Virtio_nic::Uplink_client uplink_client   { env, heap, device,
	                                            platform, config_rom.xml() };

	Main(Env & env) : env(env) {
		log("--- VirtIO MMIO NIC driver started ---"); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_nic::Main main(env);
}
