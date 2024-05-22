/*
 * \brief  Client-side event session interface
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__EVENT_SESSION__CLIENT_H_
#define _INCLUDE__EVENT_SESSION__CLIENT_H_

#include <util/noncopyable.h>
#include <event_session/event_session.h>
#include <base/attached_dataspace.h>
#include <base/rpc_client.h>
#include <input/event.h>

namespace Event { struct Session_client; }


class Event::Session_client : public Genode::Rpc_client<Session>
{
	public:

		struct Batch : Genode::Noncopyable, Genode::Interface
		{
			virtual void submit(Input::Event const &) = 0;
		};

	private:

		Genode::Attached_dataspace _ds;

		class Batch_impl : Batch
		{
			private:

				friend class Session_client;

				Session_client &_session;

				struct {
					Input::Event * const events;
					Genode::size_t const max;
				} _buffer;

				unsigned _count = 0;

				void _submit()
				{
					if (_count)
						_session.call<Rpc_submit_batch>(_count);

					_count = 0;
				}

				Batch_impl(Session_client &session)
				:
					_session(session),
					_buffer({ .events = session._ds.local_addr<Input::Event>(),
					          .max    = session._ds.size() / sizeof(Input::Event) })
				{ }

				~Batch_impl() { _submit(); }

			public:

				void submit(Input::Event const &event) override
				{
					if (_count == _buffer.max)
						_submit();

					if (_count < _buffer.max)
						_buffer.events[_count++] = event;
				}
		};


	public:

		Session_client(Genode::Region_map &local_rm,
		               Genode::Capability<Session> session)
		:
			Genode::Rpc_client<Session>(session),
			_ds(local_rm, call<Rpc_dataspace>())
		{ }

		void with_batch(auto const &fn)
		{
			Batch_impl batch { *this };

			fn(batch);
		}
};

#endif /* _INCLUDE__EVENT_SESSION__CLIENT_H_ */
