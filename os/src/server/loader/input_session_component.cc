/*
 * \brief  Input session component
 * \author Christian Prochaska
 * \date   2010-09-02
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <dataspace/client.h>
#include <input/keycodes.h>
#include <input_session/client.h>

/* local includes */
#include <nitpicker_session_component.h>
#include <input_session_component.h>


using namespace Input;


Session_component::Session_component(Genode::Rpc_entrypoint       *ep,
                                     Nitpicker::Session_component *nsc)
: _ep(ep), _nsc(nsc)
{
	/* connect to the "real" Input service */
	_isc = new (Genode::env()->heap()) Session_client(_nsc->real_input_session());
	_ev_buf = Genode::env()->rm_session()->attach(_isc->dataspace());
}


Session_component::~Session_component()
{
	Genode::env()->rm_session()->detach(_ev_buf);
	Genode::destroy(Genode::env()->heap(), _isc);
}


Genode::Dataspace_capability Session_component::dataspace()
{
	return _isc->dataspace();
}


bool Session_component::is_pending() const { return _isc->is_pending(); }


int Session_component::flush()
{
	int num_ev = _isc->flush();

	/* translate mouse position to child's coordinate system */

	int client_x, client_y, client_buf_x, client_buf_y,
	    proxy_x,  proxy_y,  proxy_buf_x,  proxy_buf_y;

	_nsc->loader_view_component()->get_viewport(&client_x, &client_y, 0, 0, &client_buf_x, &client_buf_y);
	_nsc->proxy_view_component()->get_viewport(&proxy_x, &proxy_y, 0, 0, &proxy_buf_x, &proxy_buf_y);

	if (0) {
		PDBG("app: x = %d, y = %d, buf_x = %d, buf_y = %d", client_x, client_y, client_buf_x, client_buf_y);
		PDBG("plg: x = %d, y = %d, buf_x = %d, buf_y = %d", proxy_x, proxy_y, proxy_buf_x, proxy_buf_y);
	}

	for (int i = 0; i < num_ev; i++) {
		Input::Event *real_ev = &_ev_buf[i];

		if ((real_ev->type() == Input::Event::MOTION) ||
		    (real_ev->type() == Input::Event::WHEEL) ||
		    (real_ev->keycode() == Input::BTN_LEFT) ||
		    (real_ev->keycode() == Input::BTN_RIGHT) ||
		    (real_ev->keycode() == Input::BTN_MIDDLE)) {
			Input::Event ev(real_ev->type(),
			                real_ev->keycode(),
			                real_ev->ax() - (client_x + client_buf_x) + (proxy_x + proxy_buf_x),
			                real_ev->ay() - (client_y + client_buf_y) + (proxy_y + proxy_buf_y),
			                real_ev->rx(),
			                real_ev->ry());
			*real_ev = ev;
		}
	}

	return num_ev;
}
