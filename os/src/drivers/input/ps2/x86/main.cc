/*
 * \brief  PS/2 driver for x86
 * \author Norman Feske
 * \date   2007-09-21
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <input/component.h>
#include <cap_session/connection.h>

#include "i8042.h"
#include "ps2_keyboard.h"
#include "ps2_mouse.h"
#include "irq_handler.h"

using namespace Genode;

static Event_queue ev_queue;

namespace Input {

	/*
	 * Event handling is disabled on queue creation and will be enabled later if a
	 * session is created.
	 */
	void event_handling(bool enable)
	{
		if (enable)
			ev_queue.enable();
		else
			ev_queue.disable();
	}

	bool event_pending() { return !ev_queue.empty(); }
	Event get_event() { return ev_queue.get(); }
}


int main(int argc, char **argv)
{
	I8042 i8042;

	Serial_interface *kbd = i8042.kbd_interface();
	Serial_interface *aux = i8042.aux_interface();

	Ps2_mouse    ps2_mouse(*aux, ev_queue);
	Ps2_keyboard ps2_keybd(*kbd, ev_queue, i8042.kbd_xlate());

	Irq_handler ps2_mouse_irq(12, ps2_mouse);
	Irq_handler ps2_keybd_irq( 1, ps2_keybd);

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "ps2_ep");

	/*
	 * Let the entry point serve the input root interface
	 */
	static Input::Root input_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&input_root));

	Genode::sleep_forever();
	return 0;
}
