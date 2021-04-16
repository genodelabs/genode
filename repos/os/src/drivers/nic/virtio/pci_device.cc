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

/* NIC driver includes */
#include <drivers/nic/mode.h>

/* local includes */
#include "component.h"

namespace Virtio_pci_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_pci_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Genode::Env                                  &env;
	Genode::Heap                                  heap             { env.ram(), env.rm() };
	Platform::Connection                          pci              { env };
	Platform::Device_client                       platform_device;
	Virtio::Device                                virtio_device    { env, platform_device };
	Attached_rom_dataspace                        config_rom       { env, "config" };
	Genode::Constructible<Virtio_nic::Root>       root             { };
	Genode::Constructible<Genode::Uplink_client>  uplink_client    { };

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

		Nic_driver_mode const mode {
			read_nic_driver_mode(config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:

			root.construct(env, heap, virtio_device, config_rom);

			env.parent().announce(env.ep().manage(*root));
			break;

		case Nic_driver_mode::UPLINK_CLIENT:

			uplink_client.construct(
				env, heap, virtio_device, config_rom.xml());

			break;
		}
	}
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_pci_nic::Main main(env);
}
