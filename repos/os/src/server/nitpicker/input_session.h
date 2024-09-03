/*
 * \brief  Input sub session as part of the GUI session
 * \author Norman Feske
 * \date   2017-11-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_SESSION_COMPONENT_H_
#define _INPUT_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <input_session/input_session.h>
#include <input/event.h>

/* local incudes */
#include <types.h>

namespace Input {
	using namespace Nitpicker;
	class Session_component;
}


class Input::Session_component : public Rpc_object<Session>
{
	public:

		enum { MAX_EVENTS = 200 };

		static size_t ev_ds_size() {
			return align_addr(MAX_EVENTS*sizeof(Event), 12); }

	private:

		Entrypoint &_ep;

		/*
		 * Exported event buffer dataspace
		 */
		Attached_ram_dataspace _ev_ram_ds;

		/*
		 * Local event buffer that is copied
		 * to the exported event buffer when
		 * flush() gets called.
		 */
		Event      _ev_buf[MAX_EVENTS];
		unsigned   _num_ev = 0;

		Signal_context_capability _sigh { };

	public:

		Session_component(Env &env)
		:
			_ep(env.ep()), _ev_ram_ds(env.ram(), env.rm(), ev_ds_size())
		{
			_ep.manage(*this);
		}

		~Session_component() { _ep.dissolve(*this); }

		/**
		 * Wake up client
		 */
		void submit_signal()
		{
			if (_sigh.valid())
				Signal_transmitter(_sigh).submit();
		}

		/**
		 * Enqueue event into local event buffer of the input session
		 */
		void submit(const Event *ev)
		{
			/* drop event when event buffer is full */
			if (_num_ev >= MAX_EVENTS) return;

			/* insert event into local event buffer */
			_ev_buf[_num_ev++] = *ev;

			submit_signal();
		}


		/*****************************
		 ** Input session interface **
		 *****************************/

		Dataspace_capability dataspace() override { return _ev_ram_ds.cap(); }

		bool pending() const override { return _num_ev > 0; }

		int flush() override
		{
			unsigned ev_cnt;

			/* copy events from local event buffer to exported buffer */
			Event *ev_ds_buf = _ev_ram_ds.local_addr<Event>();
			for (ev_cnt = 0; ev_cnt < _num_ev; ev_cnt++)
				ev_ds_buf[ev_cnt] = _ev_buf[ev_cnt];

			_num_ev = 0;
			return ev_cnt;
		}

		void sigh(Signal_context_capability sigh) override { _sigh = sigh; }
};

#endif /* _INPUT_SESSION_COMPONENT_H_ */

