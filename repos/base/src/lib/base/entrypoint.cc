/*
 * \brief  Entrypoint for serving RPC requests and dispatching signals
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/entrypoint.h>
#include <base/component.h>
#include <cpu/atomic.h>
#include <util/retry.h>
#include <base/rpc_client.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;

/*
 * XXX move declarations to base-internal headers
 */
namespace Genode {

	extern bool inhibit_tracing;
	void call_global_static_constructors();
}


/**
 * Return thread name used for the component's initial entrypoint
 */
static char const *initial_ep_name() { return "ep"; }


void Entrypoint::Signal_proxy_component::signal()
{
	/* signal delivered successfully */
	ep._signal_proxy_delivers_signal = false;

	ep._process_deferred_signals();

	bool io_progress = false;

	/*
	 * Try to dispatch one pending signal picked-up by the signal-proxy thread.
	 * Note, we handle only one signal here to ensure fairness between RPCs and
	 * signals.
	 */

	Signal sig = ep._sig_rec->pending_signal();

	if (sig.valid()) {
		ep._dispatch_signal(sig);

		if (sig.context()->level() == Signal_context::Level::Io) {
			/* trigger the progress handler */
			io_progress = true;
		}
	}

	if (io_progress)
		ep._handle_io_progress();
}


void Entrypoint::Signal_proxy_thread::entry()
{
	ep._process_incoming_signals();
}


void Entrypoint::_dispatch_signal(Signal &sig)
{
	Signal_dispatcher_base *dispatcher = 0;
	dispatcher = dynamic_cast<Signal_dispatcher_base *>(sig.context());

	if (!dispatcher)
		return;

	dispatcher->dispatch(sig.num());
}


void Entrypoint::_defer_signal(Signal &sig)
{
	Signal_context *context = sig.context();

	Mutex::Guard guard(_deferred_signals_mutex);
	_deferred_signals.remove(context->deferred_le());
	_deferred_signals.insert(context->deferred_le());
}


void Entrypoint::_process_deferred_signals()
{
	for (;;) {
		Signal_context *context = nullptr;
		{
			Mutex::Guard guard(_deferred_signals_mutex);
			if (!_deferred_signals.first()) return;

			context = _deferred_signals.first()->object();
			_deferred_signals.remove(_deferred_signals.first());
		}

		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(context);
		if (dispatcher) dispatcher->dispatch(1);
	}
}


void Entrypoint::_process_incoming_signals()
{
	for (;;) {

		{
			/* see documentation in 'wait_and_dispatch_one_io_signal()' */
			Mutex::Guard guard { _block_for_signal_mutex };

			_signal_proxy_delivers_signal = true;

			_sig_rec->block_for_signal();
		}

		/*
		 * It might happen that we try to forward a signal to the
		 * entrypoint, while the context of that signal is already
		 * destroyed. In that case we will get an ipc error exception
		 * as result, which has to be caught.
		 */
		try {
			_signal_proxy_cap.call<Signal_proxy::Rpc_signal>();
		} catch (Genode::Ipc_error) { /* ignore - context got destroyed in meantime */ }

		/* entrypoint destructor requested to stop signal handling */
		if (_stop_signal_proxy)
			 return;
	}
}


bool Entrypoint::_wait_and_dispatch_one_io_signal(bool const dont_block)
{
	if (!_rpc_ep->is_myself())
		warning(__func__, " called from non-entrypoint thread \"",
		       Thread::myself()->name(), "\"");

	for (;;) {

		Signal sig = _sig_rec->pending_signal();

		if (sig.valid()) {

			/* defer application-level signals */
			if (sig.context()->level() == Signal_context::Level::App) {
				_defer_signal(sig);
				continue;
			}

			_dispatch_signal(sig);
			break;
		}

		if (dont_block)
			return false;

		{
			/*
			 * The signal-proxy thread as well as the entrypoint via
			 * 'wait_and_dispatch_one_io_signal' never call
			 * 'block_for_signal()' without the '_block_for_signal_mutex'
			 * aquired. The signal-proxy thread also flags when it was
			 * unblocked by an incoming signal and delivers the signal via
			 * RPC in '_signal_proxy_delivers_signal'.
			 */
			Mutex::Guard guard { _block_for_signal_mutex };

			/*
			 * If the signal proxy is blocked in the signal-delivery
			 * RPC but the call did not yet arrive in the entrypoint
			 * (_signal_proxy_delivers_signal == true), we acknowledge the
			 * delivery here (like in 'Signal_proxy_component::signal()') and
			 * retry to fetch one pending signal at the beginning of the
			 * loop above. Otherwise, we block for the next incoming
			 * signal.
			 *
			 * There exist cases were we already processed the signal
			 * flagged in '_signal_proxy_delivers_signal' and will end
			 * up here again. In these cases we also 'block_for_signal()'.
			 */
			if (_signal_proxy_delivers_signal)
				_signal_proxy_delivers_signal = false;
			else
				_sig_rec->block_for_signal();
		}
	}

	/* initiate potential deferred-signal handling in entrypoint */
	if (_deferred_signals.first()) {
		/* construct the handler on demand (otherwise we break core) */
		if (!_deferred_signal_handler.constructed())
			_deferred_signal_handler.construct(*this, *this,
			                                   &Entrypoint::_handle_deferred_signals);
		Signal_transmitter(*_deferred_signal_handler).submit();
	}

	return true;
}


Signal_context_capability Entrypoint::manage(Signal_dispatcher_base &dispatcher)
{
	/* _sig_rec is invalid for a small window in _process_incoming_signals */
	return _sig_rec.constructed() ? _sig_rec->manage(&dispatcher)
	                              : Signal_context_capability();
}


void Genode::Entrypoint::dissolve(Signal_dispatcher_base &dispatcher)
{
	/* _sig_rec is invalid for a small window in _process_incoming_signals */
	if (_sig_rec.constructed())
		_sig_rec->dissolve(&dispatcher);

	/* also remove context from deferred signal list */
	{
		Mutex::Guard guard(_deferred_signals_mutex);
		_deferred_signals.remove(dispatcher.deferred_le());
	}
}


namespace {

	struct Constructor : Interface
	{
		GENODE_RPC(Rpc_construct, void, construct);
		GENODE_RPC_INTERFACE(Rpc_construct);
	};

	struct Constructor_component : Rpc_object<Constructor, Constructor_component>
	{
		Env &env;
		Constructor_component(Env &env) : env(env) { }

		void construct()
		{
			/* enable tracing support */
			Genode::inhibit_tracing = false;

			Genode::call_global_static_constructors();
			Genode::init_signal_transmitter(env);
			Genode::init_tracing(env);

			/*
			 * Now, as signaling is available, initialize the asynchronous
			 * parent resource mechanism
			 */
			init_parent_resource_requests(env);

			init_heartbeat_monitoring(env);

			Component::construct(env);
		}
	};

	void invoke_constructor_at_entrypoint(Capability<Constructor> cap)
	{
		cap.call<Constructor::Rpc_construct>();
	}
}


Entrypoint::Entrypoint(Env &env)
:
	_env(env),
	_rpc_ep(&env.pd(), Component::stack_size(), initial_ep_name(), Affinity::Location()),

	/* initialize signalling before creating the first signal receiver */
	_signalling_initialized((init_signal_thread(env), true))
{
	/* initialize emulation of the original synchronous root interface */
	init_root_proxy(_env);

	/*
	 * Invoke Component::construct function in the context of the entrypoint.
	 */
	Constructor_component constructor(env);

	_env.ep().manage(constructor);

	invoke_constructor_at_entrypoint(constructor.cap());

	_env.ep().dissolve(constructor);

	/*
	 * The calling initial thread becomes the signal proxy thread for this
	 * entrypoint
	 */
	_process_incoming_signals();
}


Entrypoint::Entrypoint(Env &env, size_t stack_size, char const *name,
                       Affinity::Location location)
:
	_env(env),
	_rpc_ep(&env.pd(), stack_size, name, location),
	_signalling_initialized(true)
{
	_signal_proxy_thread.construct(env, *this, location,
	                               Thread::Weight(), env.cpu());
}

Entrypoint::~Entrypoint()
{
	/* stop the signal proxy before destruction */
	_stop_signal_proxy_handler.construct(
		*this, *this, &Entrypoint::_handle_stop_signal_proxy);
	Signal_transmitter(*_stop_signal_proxy_handler).submit();
	_signal_proxy_thread->join();
	_stop_signal_proxy_handler.destruct();

	_rpc_ep->dissolve(&_signal_proxy);
}
