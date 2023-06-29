/*
 * \brief  Entrypoint for serving RPC requests and dispatching signals
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
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
#include <base/mutex.h>

namespace Genode {
	class Startup;
	class Entrypoint;
	class Env;
}


class Genode::Entrypoint : Noncopyable
{
	public:

		/**
		 * Functor for post I/O signal progress handling
		 *
		 * This mechanism is for processing I/O events deferred during signal
		 * dispatch. This is the case when the application is blocked by I/O
		 * but should not be resumed during signal dispatch.
		 */
		struct Io_progress_handler : Interface
		{
			virtual ~Io_progress_handler() {
				error("Io_progress_handler subclass cannot be safely destroyed!"); }

			virtual void handle_io_progress() = 0;
		};

	private:

		struct Signal_proxy : Interface
		{
			GENODE_RPC(Rpc_signal, void, signal);
			GENODE_RPC_INTERFACE(Rpc_signal);
		};

		struct Signal_proxy_component :
			Rpc_object<Signal_proxy, Signal_proxy_component>
		{
			Entrypoint &ep;
			Signal_proxy_component(Entrypoint &ep) : ep(ep) { }

			void signal();
		};

		struct Signal_proxy_thread : Thread
		{
			enum { STACK_SIZE = 2*1024*sizeof(long) };
			Entrypoint &ep;
			Signal_proxy_thread(Env &env, Entrypoint &ep, Location location,
			                    Weight weight, Cpu_session &cpu_session)
			:
				Thread(env, "signal_proxy", STACK_SIZE, location,
				       weight, cpu_session),
				ep(ep)
			{ start(); }

			void entry() override;
		};

		Env &_env;

		Reconstructible<Rpc_entrypoint> _rpc_ep;

		Signal_proxy_component   _signal_proxy {*this};
		Capability<Signal_proxy> _signal_proxy_cap = _rpc_ep->manage(&_signal_proxy);

		bool const _signalling_initialized;

		Reconstructible<Signal_receiver> _sig_rec { };

		Mutex                               _deferred_signals_mutex { };
		List<List_element<Signal_context> > _deferred_signals { };

		void _handle_deferred_signals() { }
		Constructible<Signal_handler<Entrypoint> > _deferred_signal_handler { };

		bool          _signal_proxy_delivers_signal { false };
		Genode::Mutex _block_for_signal_mutex       { };

		Io_progress_handler *_io_progress_handler { nullptr };

		void _handle_io_progress()
		{
			if (_io_progress_handler != nullptr)
				_io_progress_handler->handle_io_progress();
		}

		void _dispatch_signal(Signal &sig);
		void _defer_signal(Signal &sig);
		void _process_deferred_signals();
		void _process_incoming_signals();
		bool _wait_and_dispatch_one_io_signal(bool dont_block);

		Constructible<Signal_proxy_thread> _signal_proxy_thread { };
		bool                               _stop_signal_proxy { false };

		void _handle_stop_signal_proxy() { _stop_signal_proxy = true; }
		Constructible<Genode::Signal_handler<Entrypoint> > _stop_signal_proxy_handler { };

		friend class Startup;

		/**
		 * Called by the startup code only
		 */
		Entrypoint(Env &env);

		/*
		 * Noncopyable
		 */
		Entrypoint(Entrypoint const &);
		Entrypoint &operator = (Entrypoint const &);

	public:

		Entrypoint(Env &env, size_t stack_size, char const *name,
		           Affinity::Location);

		~Entrypoint();

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
		 * Block and dispatch a single I/O-level signal, return afterwards
		 *
		 * \noapi
		 *
		 * Only I/O signals are dispatched by this function. If an
		 * application-level signal occurs the dispatching of the signal is
		 * deferred until the entrypoint would block for the next time.
		 *
		 * XXX Turn into static function that ensures that the used signal
		 *     receiver belongs to the calling entrypoint. Alternatively,
		 *     remove it.
		 */
		void wait_and_dispatch_one_io_signal()
		{
			_wait_and_dispatch_one_io_signal(false);
		}

		/**
		 * Dispatch single pending I/O-level signal (non-blocking)
		 *
		 * \return true if a pending signal was dispatched, false if no signal
		 *         was pending
		 */
		bool dispatch_pending_io_signal()
		{
			return _wait_and_dispatch_one_io_signal(true);
		}

		/**
		 * Return RPC entrypoint
		 */
		Rpc_entrypoint &rpc_ep() { return *_rpc_ep; }

		/**
		 * Register hook functor to be called after I/O signals are dispatched
		 */
		void register_io_progress_handler(Io_progress_handler &handler)
		{
			if (_io_progress_handler != nullptr) {
				error("cannot call ", __func__, " twice!");
				throw Exception();
			}
			_io_progress_handler = &handler;
		}
};

#endif /* _INCLUDE__BASE__ENTRYPOINT_H_ */
