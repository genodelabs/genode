/*
 * \brief  Connection for incoming input events
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_FILTER__CONNECTION_H_
#define _INPUT_FILTER__CONNECTION_H_

/* Genode includes */
#include <input_session/connection.h>
#include <base/session_label.h>

/* local includes */
#include <types.h>

namespace Input_filter { struct Input_connection; }


class Input_filter::Input_connection
{
	public:

		struct Avail_handler { virtual void handle_input_avail() = 0; };

	private:

		Session_label const _label;
		Input::Connection   _connection;
		Attached_dataspace  _events_ds;
		Avail_handler      &_avail_handler;

		unsigned _key_cnt = 0;

		Signal_handler<Input_connection> _input_handler;

		void _handle_input() { _avail_handler.handle_input_avail(); }

		size_t _num_ev = 0;

		size_t const _max_events = _events_ds.size() / sizeof(Input::Event);

	public:

		static char const *name() { return "input"; }

		Input_connection(Env &env, Session_label const &label,
		                 Avail_handler &avail_handler, Allocator &alloc)
		:
			_label(label),
			_connection(env, label.string()),
			_events_ds(env.rm(), _connection.dataspace()),
			_avail_handler(avail_handler),
			_input_handler(env.ep(), *this, &Input_connection::_handle_input)
		{
			_connection.sigh(_input_handler);
		}

		virtual ~Input_connection() { }

		Session_label label() const { return _label; }

		template <typename FUNC>
		void for_each_event(FUNC const &func) const
		{
			Input::Event const *event_ptr = _events_ds.local_addr<Input::Event const>();

			for (size_t i = 0; i < _num_ev; i++)
				func(*event_ptr++);
		}

		void flush()
		{
			_num_ev = min(_max_events, (size_t)_connection.flush());

			auto update_key_cnt = [&] (Input::Event const &event)
			{
				if (event.type() == Input::Event::PRESS)   _key_cnt++;
				if (event.type() == Input::Event::RELEASE) _key_cnt--;
			};

			for_each_event(update_key_cnt);
		}

		bool idle() const { return _key_cnt == 0; }

		bool pending() const { return _num_ev > 0; }
};

#endif /* _INPUT_FILTER__CONNECTION_H_ */
