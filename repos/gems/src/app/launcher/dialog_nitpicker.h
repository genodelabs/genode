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
#include <base/entrypoint.h>
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

	Rpc_entrypoint &_session_ep;

	Nitpicker::Session &_nitpicker_session;

	Input::Session_client _nitpicker_input { _nitpicker_session.input_session() };

	Attached_dataspace _nitpicker_input_ds { _nitpicker_input.dataspace() };

	Signal_handler<Dialog_nitpicker_session> _input_handler;

	Input::Session_component _input_session;

	/**
	 * Constructor
	 *
	 * \param input_sigh_ep  entrypoint where the input signal handler is
	 *                       installed ('env.ep')
	 * \param service_ep     entrypoint providing the nitpicker session
	 *                       (slave-specific ep)
	 */
	Dialog_nitpicker_session(Nitpicker::Session &nitpicker_session,
	                         Entrypoint &input_sigh_ep,
	                         Rpc_entrypoint &session_ep,
	                         Input_event_handler &input_event_handler)
	:
		Wrapped_nitpicker_session(nitpicker_session),
		_input_event_handler(input_event_handler),
		_session_ep(session_ep),
		_nitpicker_session(nitpicker_session),
		_input_handler(input_sigh_ep, *this, &Dialog_nitpicker_session::_handle_input)
	{
		_session_ep.manage(this);
		_session_ep.manage(&_input_session);
		_nitpicker_input.sigh(_input_handler);

		_input_session.event_queue().enabled(true);
	}

	~Dialog_nitpicker_session()
	{
		_session_ep.dissolve(&_input_session);
		_session_ep.dissolve(this);
	}

	void _handle_input()
	{
		Input::Event const * const events =
			_nitpicker_input_ds.local_addr<Input::Event>();

		while (_nitpicker_input.pending()) {

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
		return _input_session.cap();
	}
};

#endif /* _DIALOG_NITPICKER_H_ */
