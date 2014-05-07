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

/* local includes */
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"
#include "pl050.h"

using namespace Genode;


int main(int argc, char **argv)
{
	Pl050 pl050;

	Serial_interface *kbd = pl050.kbd_interface();
	Serial_interface *aux = pl050.aux_interface();

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "ps2_ep");

	static Input::Session_component session;
	static Input::Root_component root(ep, session);

	Ps2_mouse    ps2_mouse(*aux, session.event_queue());
	Ps2_keyboard ps2_keybd(*kbd, session.event_queue(), true);

	Irq_handler ps2_mouse_irq(PL050_MOUSE_IRQ, aux, ps2_mouse);
	Irq_handler ps2_keybd_irq(PL050_KEYBD_IRQ, kbd, ps2_keybd);

	/*
	 * Let the entry point serve the input root interface
	 */
	env()->parent()->announce(ep.manage(&root));

	Genode::sleep_forever();
	return 0;
}
