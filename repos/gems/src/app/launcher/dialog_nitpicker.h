/*
 * \brief  Local nitpicker service provided to dialog slaves
 * \author Norman Feske
 * \date   2014-10-01
 *
 * This implementation of the nitpicker interface intercepts the input events
 * of a dialog slave to let the launcher respond to events (like mouse clicks)
 * directly.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DIALOG_NITPICKER_H_
#define _DIALOG_NITPICKER_H_

/* Genode includes */
#include <util/string.h>
#include <os/attached_dataspace.h>
#include <nitpicker_session/nitpicker_session.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/component.h>

/* gems includes */
#include <gems/wrapped_nitpicker_session.h>

/* local includes */
#include <types.h>

namespace Launcher {

	struct Dialog_nitpicker_session;
	struct Dialog_nitpicker_service;
}


struct Launcher::Dialog_nitpicker_session : Wrapped_nitpicker_session
{
	struct Input_event_handler
	{
		/**
		 * Handle input event
		 *
		 * \return  true if the event should be propagated to the wrapped
		 *          nitpicker session
		 */
		virtual bool handle_input_event(Input::Event const &ev) = 0;
	};

	Input_event_handler &_input_event_handler;

	Server::Entrypoint &_ep;

	Nitpicker::Session &_nitpicker_session;

	Input::Session_client _nitpicker_input { _nitpicker_session.input_session() };

	Attached_dataspace _nitpicker_input_ds { _nitpicker_input.dataspace() };

	Signal_rpc_member<Dialog_nitpicker_session>
		_input_dispatcher { _ep, *this, &Dialog_nitpicker_session::_input_handler };

	Input::Session_component _input_session;

	Capability<Input::Session> _input_session_cap { _ep.manage(_input_session) };

	/**
	 * Constructor
	 */
	Dialog_nitpicker_session(Nitpicker::Session  &nitpicker_session,
	                         Server::Entrypoint  &ep,
	                         Input_event_handler &input_event_handler)
	:
		Wrapped_nitpicker_session(nitpicker_session),
		_input_event_handler(input_event_handler),
		_ep(ep),
		_nitpicker_session(nitpicker_session)
	{
		_nitpicker_input.sigh(_input_dispatcher);

		_input_session.event_queue().enabled(true);
	}

	~Dialog_nitpicker_session()
	{
		_ep.dissolve(_input_session);
	}

	void _input_handler(unsigned)
	{
		Input::Event const * const events =
			_nitpicker_input_ds.local_addr<Input::Event>();

		while (_nitpicker_input.is_pending()) {

			size_t const num_events = _nitpicker_input.flush();
			for (size_t i = 0; i < num_events; i++) {

				Input::Event const &ev = events[i];

				if (_input_event_handler.handle_input_event(ev))
					_input_session.submit(ev);
			}
		}
	}


	/*********************************
	 ** Nitpicker session interface **
	 *********************************/

	Input::Session_capability input_session() override
	{
		return _input_session_cap;
	}
};

#endif /* _DIALOG_NITPICKER_H_ */
