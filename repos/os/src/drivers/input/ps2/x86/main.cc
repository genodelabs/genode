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
#include <base/printf.h>
#include <base/sleep.h>

/* os includes */
#include <input/component.h>
#include <input/root.h>
#include <os/server.h>
#include <pci_session/connection.h>

/* local includes */
#include "i8042.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"

using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	I8042                    i8042;
	Input::Session_component session;
	Input::Root_component    root;

	Ps2_mouse    ps2_mouse;
	Ps2_keyboard ps2_keybd;

	Pci::Connection platform;
	Pci::Device_client device_ps2;

	Irq_handler  ps2_keybd_irq;
	Irq_handler  ps2_mouse_irq;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep.rpc_ep(), session),
		ps2_mouse(*i8042.aux_interface(), session.event_queue()),
		ps2_keybd(*i8042.kbd_interface(), session.event_queue(),
		          i8042.kbd_xlate()),
		device_ps2(platform.device("PS2")),
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
	char const *name()             { return "ps2_drv_ep";      }
	size_t stack_size()            { return 1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);   }
}
