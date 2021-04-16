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

/* NIC driver includes */
#include <drivers/nic/mode.h>

/* local includes */
#include "component.h"

namespace Virtio_mmio_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_mmio_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Env                           & env;
	Heap                            heap            { env.ram(), env.rm() };
	Platform::Connection            platform        { env                 };
	Platform::Device                platform_device { platform, { "nic" } };
	Virtio::Device                  device          { platform_device     };
	Attached_rom_dataspace          config_rom      { env, "config"       };
	Constructible<Virtio_nic::Root> root            { };
	Constructible<Uplink_client>    uplink_client   { };

	Main(Env &env)
	try : env(env)
	{
		log("--- VirtIO MMIO NIC driver started ---");

		Nic_driver_mode const mode {
			read_nic_driver_mode(config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:

			root.construct( env, heap, device, config_rom);
			env.parent().announce(env.ep().manage(*root));
			break;

		case Nic_driver_mode::UPLINK_CLIENT:

			uplink_client.construct( env, heap, device, config_rom.xml());
			break;
		}
	}
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_nic::Main main(env);
}
