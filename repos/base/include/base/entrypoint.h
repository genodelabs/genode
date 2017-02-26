/*
 * \brief  Entrypoint for serving RPC requests and dispatching signals
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ENTRYPOINT_H_
#define _INCLUDE__BASE__ENTRYPOINT_H_

#include <util/reconstructible.h>
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
	public:

		/**
		 * Functor for post signal-handler hook
		 */
		struct Post_signal_hook { virtual void function() = 0; };

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
				/* XXX introduce while-pending loop */
				try {
					Signal sig = ep._sig_rec->pending_signal();
					ep._dispatch_signal(sig);
				} catch (Signal_receiver::Signal_not_pending) { }

				ep._execute_post_signal_hook();
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

		Reconstructible<Rpc_entrypoint> _rpc_ep;

		Signal_proxy_component   _signal_proxy {*this};
		Capability<Signal_proxy> _signal_proxy_cap = _rpc_ep->manage(&_signal_proxy);

		Reconstructible<Signal_receiver> _sig_rec;

		bool _suspended                = false;
		void (*_suspended_callback) () = nullptr;
		void (*_resumed_callback)   () = nullptr;

		enum Signal_recipient {
			NONE = 0, ENTRYPOINT = 1, SIGNAL_PROXY = 2
		};

		int               _signal_recipient { NONE };
		Genode::Lock      _signal_pending_lock;
		Post_signal_hook *_post_signal_hook = nullptr;

		void _execute_post_signal_hook()
		{
			if (_post_signal_hook != nullptr)
				_post_signal_hook->function();

			_post_signal_hook = nullptr;
		}

		/*
		 * This signal handler is solely used to force an iteration of the
		 * signal-dispatch loop. It is triggered by 'schedule_suspend' to
		 * let the signal-dispatching thread execute the actual suspend-
		 * resume mechanism.
		 */
		void _handle_suspend() { _suspended = true; }
		Constructible<Genode::Signal_handler<Entrypoint>> _suspend_dispatcher;

		void _dispatch_signal(Signal &sig);

		void _process_incoming_signals();

		Constructible<Signal_proxy_thread> _signal_proxy_thread;

		friend class Startup;


		/**
		 * Called by the startup code only
		 */
		Entrypoint(Env &env);

	public:

		Entrypoint(Env &env, size_t stack_size, char const *name);

		~Entrypoint()
		{
			_rpc_ep->dissolve(&_signal_proxy);
		}

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
		void wait_and_dispatch_one_signal();

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

		/**
		 * Register hook functor to be called after signal was handled
		 */
		void schedule_post_signal_hook(Post_signal_hook *hook)
		{
			_post_signal_hook = hook;
		}
};

#endif /* _INCLUDE__BASE__ENTRYPOINT_H_ */
