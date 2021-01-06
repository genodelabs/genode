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

	struct Device_info {
		using String = Genode::String<64>;

		String name {};
		size_t io_mem_offset = 0;

		Device_info(Platform::Connection & platform) {
			bool found = false;
			platform.with_xml([&] (Xml_node & xml) {
				xml.for_each_sub_node("device", [&] (Xml_node node) {
					if (found) return;

					node.for_each_sub_node("property", [&] (Xml_node node) {
						if ((node.attribute_value("name", String()) == "type") &&
						    (node.attribute_value("value", String()) == "nic")) {
							found = true; }
					});

					if (!found) return;

					name = node.attribute_value("name", String());

					node.for_each_sub_node("io_mem", [&] (Xml_node node) {
						io_mem_offset = node.attribute_value<size_t>("page_offset", 0);
					});
				});
			});
			if (!found) {
				error("No device was found");
				throw Device_not_found();
			}
		}
	};

	Genode::Env                                  &env;
	Genode::Heap                                  heap            { env.ram(), env.rm() };
	Platform::Connection                          platform        { env };
	Device_info                                   info            { platform };
	Platform::Device_client                       platform_device { platform.acquire_device(info.name.string()) };
	Virtio::Device                                virtio_device   { env, platform_device.io_mem_dataspace(),
	                                                                info.io_mem_offset };
	Attached_rom_dataspace                        config_rom      { env, "config" };
	Genode::Constructible<Virtio_nic::Root>       root            { };
	Genode::Constructible<Genode::Uplink_client>  uplink_client   { };

	Main(Env &env)
	try : env(env)
	{
		log("--- VirtIO MMIO NIC driver started ---");

		Nic_driver_mode const mode {
			read_nic_driver_mode(config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:

			root.construct(
				env, heap, virtio_device, platform_device.irq(0),
				config_rom);

			env.parent().announce(env.ep().manage(*root));
			break;

		case Nic_driver_mode::UPLINK_CLIENT:

			uplink_client.construct(
				env, heap, virtio_device, platform_device.irq(0),
				config_rom.xml());

			break;
		}
	}
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_nic::Main main(env);
}
