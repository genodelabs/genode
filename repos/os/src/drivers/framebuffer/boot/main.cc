/*
 * \brief  Framebuffer driver that uses a framebuffer supplied by the core rom.
 * \author Johannes Kliemann
 * \date   2017-06-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <dataspace/capability.h>
#include <os/static_root.h>

#include <framebuffer.h>

struct Main {

	Genode::Env &env;

	Genode::Attached_rom_dataspace pinfo {
		env,
		"platform_info"
	};

	Framebuffer::Session_component fb {
		env,
		pinfo.xml(),
	};

	Genode::Static_root<Framebuffer::Session> fb_root {env.ep().manage(fb)};

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(fb_root));
	}
};

void Component::construct(Genode::Env &env) {

	static Main inst(env);

}
