/*
 * \brief  'Audio_out' part of black hole component
 * \author Christian Prochaska
 * \date   2021-07-07
 *
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AUDIO_OUT_H_
#define _AUDIO_OUT_H_

/* Genode includes */
#include <audio_out_session/rpc_object.h>
#include <timer_session/connection.h>


namespace Audio_out
{
	class Session_component_base;
	class Session_component;
	class Root;
}


/**
 * This base struct is needed to get the 'Signal_handler' constructed
 * before the 'Session_rpc_object' constructor is called with the handler
 * as argument.
 */

struct Audio_out::Session_component_base
{
	Genode::Signal_handler<Session_component_base>  _data_available_handler;
	Timer::One_shot_timeout<Session_component_base> _timeout;

	virtual void _handle_data_available() = 0;
	virtual void _handle_timeout(Genode::Duration) = 0;

	Session_component_base(Genode::Env &env, Timer::Connection &timer)
	: _data_available_handler(env.ep(),
		                      *this,
		                      &Session_component_base::_handle_data_available),
	  _timeout(timer, *this, &Session_component_base::_handle_timeout)
	{ }

	virtual ~Session_component_base() { }
};


class Audio_out::Session_component : Audio_out::Session_component_base,
                                     public Audio_out::Session_rpc_object
{
	private:

		/* subtract 200ms for overhead */
		Genode::Microseconds _delay {
			((Audio_out::PERIOD * 1000 * 1000) / Audio_out::SAMPLE_RATE) - 200 };

		void _handle_data_available() override
		{
			_timeout.schedule(_delay);
		}

		void _handle_timeout(Genode::Duration) override
		{
			if (stream()->empty())
				return;

			bool const full = stream()->full();

			Packet *p = stream()->get(stream()->pos());
			p->invalidate();
			p->mark_as_played();
			stream()->increment_position();
			progress_submit();

			if (full) alloc_submit();

			if (!stream()->empty())
				_timeout.schedule(_delay);
		}

	public:

		Session_component(Genode::Env &env, Timer::Connection &timer)
		: Session_component_base(env, timer),
		  Session_rpc_object(env, _data_available_handler)
		{ }

		~Session_component()
		{
			if (Session_rpc_object::active()) stop();
		}

		void start() override
		{
			Session_rpc_object::start();
			stream()->pos(0);
		}

		void stop() override
		{
			Session_rpc_object::stop();
		}
};


namespace Audio_out {
	typedef Genode::Root_component<Session_component, Genode::Multiple_clients> Root_component;
}


class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Genode::Env &_env;
		Timer::Connection _timer;

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			if (sizeof(Stream) > ram_quota) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", sizeof(Stream));
				throw Insufficient_ram_quota();
			}

			Session_component *session = new (md_alloc())
				Session_component(_env, _timer);

			return session;

		}

		void _destroy_session(Session_component *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

	public:

		Root(Genode::Env        &env,
		     Genode::Allocator  &md_alloc)
		: Root_component(env.ep(), md_alloc),
		  _env(env), _timer(env) { }
};

#endif /* _AUDIO_OUT_H_ */
