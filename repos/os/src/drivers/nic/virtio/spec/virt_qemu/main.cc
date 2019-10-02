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

#include <base/component.h>
#include <base/heap.h>
#include <nic/xml_node.h>
#include <platform_session/connection.h>
#include <virtio/device.h>

#include "component.h"

namespace Virtio_mmio_nic {
	using namespace Genode;
	struct Main;
}

struct Virtio_mmio_nic::Main
{
	struct Device_not_found : Genode::Exception { };

	Genode::Env            &env;
	Genode::Heap            heap          { env.ram(), env.rm() };
	Platform::Connection    platform      { env };
	off_t                   io_mem_offset { 0 };
	Platform::Device_client platform_device;
	Virtio::Device          virtio_device;
	Virtio_nic::Root        root { env, heap, virtio_device, platform_device.irq() };

	Platform::Device_capability find_platform_device()
	{
		using String = Genode::String<64>;

		Platform::Device_capability cap;

		platform.with_xml([&] (Xml_node const &xml) {
			xml.for_each_sub_node("device", [&] (Xml_node const &device_node) {
				bool found = false;
				off_t offset = 0;

				device_node.for_each_sub_node("property", [&] (Xml_node const &node) {
					if ((node.attribute_value("name", String()) == "type") &&
					    (node.attribute_value("value", String()) == "nic")) {
						found = true;
					}
					if ((node.attribute_value("name", String()) == "io_mem_offset")) {
						offset = node.attribute_value<off_t>("value", 0);
					}
				});

				if (!found) return;

				auto name = device_node.attribute_value("name", Platform::Device::Name());
				cap = platform.acquire_device(name.string());
				io_mem_offset = offset;
			});
		});

		if (!cap.valid()) {
			Genode::error("No VirtIO MMIO NIC device found!");
			throw Device_not_found();
		}

		return cap;
	}

	Main(Env &env)
	try : env(env),
	      platform_device(find_platform_device()),
	      virtio_device(env, platform_device.io_mem_dataspace(), io_mem_offset)
	{
		log("--- VirtIO MMIO NIC driver started ---");
		env.parent().announce(env.ep().manage(root));
	}
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_nic::Main main(env);
}

