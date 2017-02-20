/*
 * \brief  Connection to timer service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TIMER_SESSION__CONNECTION_H_
#define _INCLUDE__TIMER_SESSION__CONNECTION_H_

#include <timer_session/client.h>
#include <base/connection.h>

namespace Timer { class Connection; }


class Timer::Connection : public Genode::Connection<Session>, public Session_client
{
	private:

		Genode::Lock            _lock;
		Genode::Signal_receiver _sig_rec;
		Genode::Signal_context  _default_sigh_ctx;

		Genode::Signal_context_capability
			_default_sigh_cap = _sig_rec.manage(&_default_sigh_ctx);

		Genode::Signal_context_capability _custom_sigh_cap;

	public:

		/**
		 * Constructor
		 */
		Connection(Genode::Env &env, char const *label = "")
		:
			Genode::Connection<Session>(env, session(env.parent(), "ram_quota=10K, label=\"%s\"", label)),
			Session_client(cap())
		{
			/* register default signal handler */
			Session_client::sigh(_default_sigh_cap);
		}

		/**
		 * Constructor
		 *
		 * \noapi
		 * \deprecated  Use the constructor with 'Env &' as first
		 *              argument instead
		 */
		Connection() __attribute__((deprecated))
		:
			Genode::Connection<Session>(session("ram_quota=10K")),
			Session_client(cap())
		{
			/* register default signal handler */
			Session_client::sigh(_default_sigh_cap);
		}

		~Connection() { _sig_rec.dissolve(&_default_sigh_ctx); }

		/*
		 * Intercept 'sigh' to keep track of customized signal handlers
		 */
		void sigh(Signal_context_capability sigh)
		{
			_custom_sigh_cap = sigh;
			Session_client::sigh(_custom_sigh_cap);
		}

		void usleep(unsigned us)
		{
			/*
			 * Omit the interaction with the timer driver for the corner case
			 * of not sleeping at all. This corner case may be triggered when
			 * polling is combined with sleeping (as some device drivers do).
			 * If we passed the sleep operation to the timer driver, the
			 * timer would apply its policy about a minimum sleep time to
			 * the sleep operation, which is not desired when polling.
			 */
			if (us == 0)
				return;

			/* serialize sleep calls issued by different threads */
			Genode::Lock::Guard guard(_lock);

			/* temporarily install to the default signal handler */
			if (_custom_sigh_cap.valid())
				Session_client::sigh(_default_sigh_cap);

			/* trigger timeout at default signal handler */
			trigger_once(us);
			_sig_rec.wait_for_signal();

			/* revert custom signal handler if registered */
			if (_custom_sigh_cap.valid())
				Session_client::sigh(_custom_sigh_cap);
		}

		void msleep(unsigned ms)
		{
			usleep(1000*ms);
		}
};

#endif /* _INCLUDE__TIMER_SESSION__CONNECTION_H_ */
