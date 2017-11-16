/*
 * \brief  Time source that uses an extra thread for timeout handling
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _THREADED_TIME_SOURCE_H_
#define _THREADED_TIME_SOURCE_H_

/* Genode inludes */
#include <base/env.h>
#include <base/rpc_client.h>
#include <timer/timeout.h>

namespace Timer {

	using Genode::Microseconds;
	using Genode::Duration;
	class Threaded_time_source;
}


class Timer::Threaded_time_source : public Genode::Time_source,
                                    protected Genode::Thread
{
	private:

		struct Irq_dispatcher
		{
			GENODE_RPC(Rpc_do_dispatch, void, do_dispatch);
			GENODE_RPC_INTERFACE(Rpc_do_dispatch);
		};

		struct Irq_dispatcher_component : Genode::Rpc_object<Irq_dispatcher,
		                                                     Irq_dispatcher_component>
		{
				Timeout_handler *handler = nullptr;
				Threaded_time_source &ts;

				Irq_dispatcher_component(Threaded_time_source &ts) : ts(ts) { }

				/********************
				 ** Irq_dispatcher **
				 ********************/

				void do_dispatch()
				{
					/* call curr_time in ep and not in ts (no locks in use!) */
					ts._irq = true;
					Duration us = ts.curr_time();
					ts._irq = false;

					if (handler)
						handler->handle_timeout(us);
				}

		} _irq_dispatcher_component;

		Genode::Capability<Irq_dispatcher> _irq_dispatcher_cap;

		virtual void _wait_for_irq() = 0;

		/***********************
		 ** Thread_deprecated **
		 ***********************/

		void entry()
		{
			while (true) {
				_wait_for_irq();
				_irq_dispatcher_cap.call<Irq_dispatcher::Rpc_do_dispatch>();
			}
		}

	protected:

		bool _irq { false };

	public:

		Threaded_time_source(Genode::Env &env)
		:
			Thread(env, "threaded_time_source", 8 * 1024 * sizeof(Genode::addr_t)),
			_irq_dispatcher_component(*this),
			_irq_dispatcher_cap(env.ep().rpc_ep().manage(&_irq_dispatcher_component))
		{ }

		void handler(Timeout_handler &handler) {
			_irq_dispatcher_component.handler = &handler; }
};

#endif /* _THREADED_TIME_SOURCE_H_ */
