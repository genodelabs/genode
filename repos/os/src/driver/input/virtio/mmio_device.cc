/*
 * \brief  VirtIO MMIO input driver
 * \author Piotr Tworek
 * \date   2020-02-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <platform_session/connection.h>
#include <virtio/mmio_device.h>

#include "component.h"

namespace Virtio_mmio_input {
	using namespace Genode;
	struct Main;
}


struct Virtio_mmio_input::Main
{
	Env                    &env;
	Platform::Connection    platform        { env };
	Platform::Device        platform_device { platform,
	                                          Platform::Device::Type { "input" } };
	Virtio::Device          virtio_device   { platform_device };
	Attached_rom_dataspace  config          { env, "config" };
	Virtio_input::Driver    driver          { env, platform, virtio_device,
	                                          config.xml() };

	Main(Env &env)
	try : env(env) { log("--- VirtIO MMIO input driver started ---"); }
	catch (...) { env.parent().exit(-1); }
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_input::Main main(env);
}
