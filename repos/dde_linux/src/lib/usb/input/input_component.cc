/*
 * \brief   Linux 2.6 Input driver for USB HID
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \author  Sebastian Sumpf
 * \date    2011-07-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/rpc_server.h>
#include <input/root.h>
#include <os/ring_buffer.h>

#include <lx_emul.h>
#include "platform.h"

#undef RELEASE

using namespace Genode;


/**
 * Return singleton instance of input-session component
 */
static Input::Session_component &input_session()
{
	static Input::Session_component inst;
	return inst;
}


/**
 * Return singleton instance of input-root component
 *
 * On the first call (from 'start_input_service'), the 'ep' argument is
 * specified. All subsequent calls (from 'input_callback') just return the
 * reference to the singleton instance.
 */
static Input::Root_component &input_root(Rpc_entrypoint *ep = 0)
{
	static Input::Root_component root(*ep, input_session());
	return root;
}


/**
 * Input event call-back function
 */
static void input_callback(enum input_event_type type,
                           unsigned code,
                           int absolute_x, int absolute_y,
                           int relative_x, int relative_y)
{
	Input::Event::Type t = Input::Event::INVALID;
	switch (type) {
		case EVENT_TYPE_PRESS:   t = Input::Event::PRESS; break;
		case EVENT_TYPE_RELEASE: t = Input::Event::RELEASE; break;
		case EVENT_TYPE_MOTION:  t = Input::Event::MOTION; break;
		case EVENT_TYPE_WHEEL:   t = Input::Event::WHEEL; break;
		case EVENT_TYPE_TOUCH:   t = Input::Event::TOUCH; break;
	}

	input_session().submit(Input::Event(t, code,
	                                    absolute_x, absolute_y,
	                                    relative_x, relative_y));
}


void start_input_service(void *ep_ptr, void * service_ptr)
{
	Rpc_entrypoint *ep = static_cast<Rpc_entrypoint *>(ep_ptr);
	Services *service = static_cast<Services *>(service_ptr);
	Env &env = service->env;

	env.parent().announce(ep->manage(&input_root(ep)));

	genode_input_register(input_callback, service->screen_width,
	                      service->screen_height, service->multitouch);
}
