/*
 * \brief   Linux 2.6 Input driver for USB HID
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \author  Sebastian Sumpf
 * \date    2011-07-15
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/rpc_server.h>
#include <input/component.h>
#include <os/ring_buffer.h>

#include <lx_emul.h>

#undef RELEASE

using namespace Genode;


/*********************
 ** Input component **
 *********************/

typedef Ring_buffer<Input::Event, 512> Input_ring_buffer;

static Input_ring_buffer ev_queue;

namespace Input {

	void event_handling(bool enable) { }
	bool event_pending() { return !ev_queue.empty(); }
	Event get_event()    { return ev_queue.get(); }
}


/**
 * Input event call-back function
 */
static void input_callback(enum input_event_type type,
                           unsigned keycode,
                           int absolute_x, int absolute_y,
                           int relative_x, int relative_y)
{
	Input::Event::Type t = Input::Event::INVALID;
	switch (type) {
		case EVENT_TYPE_PRESS:   t = Input::Event::PRESS; break;
		case EVENT_TYPE_RELEASE: t = Input::Event::RELEASE; break;
		case EVENT_TYPE_MOTION:  t = Input::Event::MOTION; break;
		case EVENT_TYPE_WHEEL:   t = Input::Event::WHEEL; break;
	}

	try {
		ev_queue.add(Input::Event(t, keycode,
		                          absolute_x, absolute_y,
		                          relative_x, relative_y));
	} catch (Input_ring_buffer::Overflow) {
		PWRN("input ring buffer overflow");
	}
}


void start_input_service(void *ep)
{
	Rpc_entrypoint *e = static_cast<Rpc_entrypoint *>(ep);
	static Input::Root input_root(e, env()->heap());
	env()->parent()->announce(e->manage(&input_root));

	genode_input_register(input_callback);
}
