/*
 * \brief   Linux 2.6 Input driver for USB HID
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \author  Sebastian Sumpf
 * \date    2011-07-15
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/rpc_server.h>
#include <event_session/connection.h>
#include <os/ring_buffer.h>

#include <lx_emul.h>
#include "platform.h"

#undef RELEASE

using namespace Genode;


/**
 * Singleton instance of input-session component
 */
static Genode::Constructible<Event::Connection> _event_session;


/**
 * Input event call-back function
 */
static void input_callback(enum input_event_type type,
                           unsigned code, int ax, int ay, int rx, int ry)
{
	auto submit = [&] (Input::Event const &ev) {
		_event_session->with_batch([&] (Event::Session_client::Batch &batch) {
			batch.submit(ev); });
	};

	using namespace Input;

	switch (type) {
	case EVENT_TYPE_PRESS:   submit(Press{Keycode(code)});    break;
	case EVENT_TYPE_RELEASE: submit(Release{Keycode(code)});  break;
	case EVENT_TYPE_MOTION:
		if (rx == 0 && ry == 0)
			submit(Absolute_motion{ax, ay});
		else
			submit(Relative_motion{rx, ry});
		break;
	case EVENT_TYPE_WHEEL:   submit(Wheel{rx, ry});           break;
	case EVENT_TYPE_TOUCH:
		{
			Touch_id const id { (int)code };

			if (rx == -1 && ry == -1)
				submit(Touch_release{id});
			else
				submit(Touch{id, (float)ax, (float)ay});
			break;
		}
	}
}


void start_input_service(void *service_ptr)
{
	Services *service = static_cast<Services *>(service_ptr);
	Env &env = service->env;

	_event_session.construct(env);

	genode_input_register(input_callback, service->screen_width,
	                      service->screen_height, service->multitouch);
}
