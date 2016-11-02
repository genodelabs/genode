/*
 * \brief  Entrypoint for serving RPC requests and dispatching signals
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ENTRYPOINT_H_
#define _INCLUDE__BASE__ENTRYPOINT_H_

#include <util/volatile_object.h>
#include <util/noncopyable.h>
#include <base/rpc_server.h>
#include <base/signal.h>
#include <base/thread.h>


namespace Genode {
	class Startup;
	class Entrypoint;
	class Env;
}


class Genode::Entrypoint : Genode::Noncopyable
{
	private:

		struct Signal_proxy
		{
			GENODE_RPC(Rpc_signal, void, signal);
			GENODE_RPC_INTERFACE(Rpc_signal);
		};

		struct Signal_proxy_component :
			Rpc_object<Signal_proxy, Signal_proxy_component>
		{
			Entrypoint &ep;
			Signal_proxy_component(Entrypoint &ep) : ep(ep) { }

			void signal()
			{
				try {
					Signal sig = ep._sig_rec->pending_signal();
					ep._dispatch_signal(sig);
				} catch (Signal_receiver::Signal_not_pending) { }
			}
		};

		struct Signal_proxy_thread : Thread
		{
			enum { STACK_SIZE = 2*1024*sizeof(long) };
			Entrypoint &ep;
			Signal_proxy_thread(Env &env, Entrypoint &ep)
			:
				Thread(env, "signal_proxy", STACK_SIZE),
				ep(ep)
			{ start(); }

			void entry() override { ep._process_incoming_signals(); }
		};

		Env &_env;

		Volatile_object<Rpc_entrypoint> _rpc_ep;

		Signal_proxy_component   _signal_proxy {*this};
		Capability<Signal_proxy> _signal_proxy_cap = _rpc_ep->manage(&_signal_proxy);

		Volatile_object<Signal_receiver> _sig_rec;

		void (*_suspended_callback) () = nullptr;
		void (*_resumed_callback)   () = nullptr;

		/*
		 * This signal handler is solely used to force an iteration of the
		 * signal-dispatch loop. It is triggered by 'schedule_suspend' to
		 * let the signal-dispatching thread execute the actual suspend-
		 * resume mechanism.
		 */
		void _handle_suspend() { }
		Lazy_volatile_object<Genode::Signal_handler<Entrypoint>> _suspend_dispatcher;

		void _dispatch_signal(Signal &sig);

		void _process_incoming_signals();

		Lazy_volatile_object<Signal_proxy_thread> _signal_proxy_thread;

		friend class Startup;


		/**
		 * Called by the startup code only
		 */
		Entrypoint(Env &env);

	public:

		Entrypoint(Env &env, size_t stack_size, char const *name);

		/**
		 * Associate RPC object with the entry point
		 */
		template <typename RPC_INTERFACE, typename RPC_SERVER>
		Capability<RPC_INTERFACE>
		manage(Rpc_object<RPC_INTERFACE, RPC_SERVER> &obj)
		{
			return _rpc_ep->manage(&obj);
		}

		/**
		 * Dissolve RPC object from entry point
		 */
		template <typename RPC_INTERFACE, typename RPC_SERVER>
		void dissolve(Rpc_object<RPC_INTERFACE, RPC_SERVER> &obj)
		{
			_rpc_ep->dissolve(&obj);
		}

		/**
		 * Associate signal dispatcher with entry point
		 */
		Signal_context_capability manage(Signal_dispatcher_base &);

		/**
		 * Disassociate signal dispatcher from entry point
		 */
		void dissolve(Signal_dispatcher_base &);

		/**
		 * Block and dispatch a single signal, return afterwards
		 *
		 * \noapi
		 *
		 * XXX Turn into static function that ensures that the used signal
		 *     receiver belongs to the calling entrypoint. Alternatively,
		 *     remove it.
		 */
		void wait_and_dispatch_one_signal()
		{
			Signal sig = _sig_rec->wait_for_signal();
			_dispatch_signal(sig);
		}

		/**
		 * Return RPC entrypoint
		 */
		Rpc_entrypoint &rpc_ep() { return *_rpc_ep; }

		/**
		 * Trigger a suspend-resume cycle in the entrypoint
		 *
		 * 'suspended' is called after the entrypoint entered the safe suspend
		 * state, while 'resumed is called when the entrypoint is fully functional again.
		 */
		void schedule_suspend(void (*suspended)(), void (*resumed)());
};

#endif /* _INCLUDE__BASE__ENTRYPOINT_H_ */
