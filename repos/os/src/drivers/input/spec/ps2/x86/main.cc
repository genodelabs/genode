/*
 * \brief  PS/2 driver for x86
 * \author Norman Feske
 * \date   2007-09-21
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/env.h>
#include <util/retry.h>

/* os includes */
#include <input/component.h>
#include <input/root.h>
#include <os/config.h>
#include <os/server.h>
#include <platform_session/connection.h>

/* local includes */
#include "i8042.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"

using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	Input::Session_component session;
	Input::Root_component    root;

	Platform::Connection    platform;

	Platform::Device_capability cap_ps2()
	{
		return Genode::retry<Platform::Session::Out_of_metadata>(
			[&] () { return platform.device("PS2"); },
			[&] () { platform.upgrade_ram(4096); });
	}

	Platform::Device_client device_ps2;

	I8042              i8042;

	Ps2_keyboard ps2_keybd;
	Ps2_mouse    ps2_mouse;

	Irq_handler  ps2_keybd_irq;
	Irq_handler  ps2_mouse_irq;

	enum { REG_IOPORT_DATA = 0, REG_IOPORT_STATUS};

	bool _check_verbose(const char * verbose)
	{
		return Genode::config()->xml_node().attribute_value(verbose, false);
	}

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep.rpc_ep(), session),
		device_ps2(cap_ps2()),
		i8042(device_ps2.io_port(REG_IOPORT_DATA),
		      device_ps2.io_port(REG_IOPORT_STATUS)),
		ps2_keybd(*i8042.kbd_interface(), session.event_queue(),
		          i8042.kbd_xlate(), _check_verbose("verbose_keyboard"),
		          _check_verbose("verbose_scancodes")),
		ps2_mouse(*i8042.aux_interface(), session.event_queue(),
		          _check_verbose("verbose_mouse")),
		ps2_keybd_irq(ep, ps2_keybd, device_ps2.irq(0)),
		ps2_mouse_irq(ep, ps2_mouse, device_ps2.irq(1))
	{
		env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "ps2_drv_ep";        }
	size_t stack_size()            { return 8*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
