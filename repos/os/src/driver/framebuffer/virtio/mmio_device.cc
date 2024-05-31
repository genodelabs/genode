/*
 * \brief  VirtIO MMIO Framebuffer driver
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
#include <platform_session/dma_buffer.h>
#include <virtio/mmio_device.h>

#include "component.h"

namespace Virtio_mmio_fb {
    using namespace Genode;
    struct Main;
}

struct Virtio_mmio_fb::Main
{
	Genode::Env            &env;
	Platform::Connection    platform        { env };
	Platform::Device        platform_device { platform,
	                                          Platform::Device::Type { "gpu" } };
	Virtio::Device          virtio_device   { platform_device };
	Virtio_fb::Driver       driver          { env, platform, virtio_device };

	Main(Env &env)
	try : env(env) { log("--- VirtIO MMIO Framebuffer driver started ---"); }
	catch (...) { env.parent().exit(-1); }
};

void Component::construct(Genode::Env &env)
{
	static Virtio_mmio_fb::Main main(env);
}
