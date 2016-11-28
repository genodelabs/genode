/*
 * \brief  PS/2 driver for PL050
 * \author Norman Feske
 * \date   2007-09-21
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <input/component.h>
#include <input/root.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <os/server.h>

/* local includes */
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "pl050.h"

using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	Pl050                    pl050;
	Input::Session_component session;
	Input::Root_component    root;

	Ps2_mouse    ps2_mouse;
	Ps2_keyboard ps2_keybd;

	Irq_handler  ps2_mouse_irq;
	Irq_handler  ps2_keybd_irq;

	bool _check_verbose(const char * verbose)
	{
		return Genode::config()->xml_node().attribute_value(verbose, false);
	}

	Main(Server::Entrypoint &ep)
		: ep(ep), root(ep.rpc_ep(), session),
		ps2_mouse(*pl050.aux_interface(), session.event_queue(),
		          _check_verbose("verbose_mouse")),
		ps2_keybd(*pl050.kbd_interface(), session.event_queue(), true,
		          _check_verbose("verbose_keyboard"),
		          _check_verbose("verbose_scancodes")),
		ps2_mouse_irq(ep, PL050_MOUSE_IRQ, pl050.aux_interface(), ps2_mouse),
		ps2_keybd_irq(ep, PL050_KEYBD_IRQ, pl050.kbd_interface(), ps2_keybd)
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
