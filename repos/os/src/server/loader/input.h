/*
 * \brief  Virtualized input session
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2010-09-02
 *
 * This input service implementation is used by the virtualized nitpicker
 * service to translate the input coordinate system from the coordinates
 * seen by the user of the virtualized session and the physical coordinates
 * dictated by the loader-session client.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INPUT_H_
#define _INPUT_H_

#include <base/rpc_server.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <input_session/client.h>

namespace Input {

	using namespace Genode;

	typedef Genode::Point<> Motion_delta;

	class Session_component;
}


class Input::Session_component : public Rpc_object<Session>
{
	private:

		Session_client _real_input;
		Motion_delta  &_motion_delta;
		Event  * const _ev_buf;

		Genode::Signal_context_capability _sigh;

	public:

		/**
		 * Constructor
		 */
		Session_component(Session_capability real_input,
		                  Motion_delta      &motion_delta)
		:
			_real_input(real_input), _motion_delta(motion_delta),
			_ev_buf(env()->rm_session()->attach(_real_input.dataspace()))
		{ }

		/**
		 * Destructor
		 */
		~Session_component() { env()->rm_session()->detach(_ev_buf); }


		/*****************************
		 ** Input session interface **
		 *****************************/

		Dataspace_capability dataspace() override { return _real_input.dataspace(); }

		bool pending() const override { return _real_input.pending(); }

		int flush() override
		{
			/* translate mouse position to child's coordinate system */
			Motion_delta const delta = _motion_delta;

			int const num_ev = _real_input.flush();
			for (int i = 0; i < num_ev; i++) {

				Input::Event &ev = _ev_buf[i];

				if ((ev.type() == Input::Event::MOTION)
				 || (ev.type() == Input::Event::WHEEL)
				 || (ev.code() == Input::BTN_LEFT)
				 || (ev.code() == Input::BTN_RIGHT)
				 || (ev.code() == Input::BTN_MIDDLE)) {

					ev = Input::Event(ev.type(),
					                  ev.code(),
					                  ev.ax() + delta.x(),
					                  ev.ay() + delta.y(),
					                  ev.rx(),
					                  ev.ry());
				}
			}

			return num_ev;
		}

		void sigh(Signal_context_capability sigh) override
		{
			/*
			 * Maintain local copy of signal-context capability to keep
			 * NOVA from flushing transitive delegations of the capability.
			 */
			_sigh = sigh;

			_real_input.sigh(sigh);
		}
};

#endif /* _INPUT_H_ */
