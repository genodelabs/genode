/*
 * \brief  Entrypoint for serving RPC requests and dispatching signals
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/entrypoint.h>
#include <base/component.h>
#include <cap_session/connection.h>
#include <util/retry.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;

/*
 * XXX move declarations to base-internal headers
 */
namespace Genode {

	extern bool inhibit_tracing;
	void call_global_static_constructors();
	void destroy_signal_thread();

	extern void (*call_component_construct)(Genode::Env &);
}


/**
 * Return thread name used for the component's initial entrypoint
 */
static char const *initial_ep_name() { return "ep"; }


void Entrypoint::_dispatch_signal(Signal &sig)
{
	Signal_dispatcher_base *dispatcher = 0;
	dispatcher = dynamic_cast<Signal_dispatcher_base *>(sig.context());

	if (!dispatcher)
		return;

	dispatcher->dispatch(sig.num());
}


void Entrypoint::_process_incoming_signals()
{
	for (;;) {

		do {
			_sig_rec->block_for_signal();

			/*
			 * It might happen that we try to forward a signal to the
			 * entrypoint, while the context of that signal is already
			 * destroyed. In that case we will get an ipc error exception
			 * as result, which has to be caught.
			 */
			retry<Genode::Blocking_canceled>(
				[&] () { _signal_proxy_cap.call<Signal_proxy::Rpc_signal>(); },
				[]  () { warning("blocking canceled during signal processing"); }
			);

		} while (!_suspended_callback);

		_suspend_dispatcher.destruct();
		_sig_rec.destruct();
		dissolve(_signal_proxy);
		_signal_proxy_cap = Capability<Signal_proxy>();
		_rpc_ep.destruct();
		destroy_signal_thread();

		/* execute fork magic in noux plugin */
		_suspended_callback();

		init_signal_thread(_env);

		_rpc_ep.construct(&_env.pd(), Component::stack_size(), initial_ep_name());
		_signal_proxy_cap = manage(_signal_proxy);
		_sig_rec.construct();

		/*
		 * Before calling the resumed callback, we reset the callback pointer
		 * as these may be set again in the resumed code to initiate the next
		 * suspend-resume cycle (e.g., exit()).
		 */
		void (*resumed_callback)() = _resumed_callback;
		_suspended_callback        = nullptr;
		_resumed_callback          = nullptr;

		resumed_callback();
	}
}


void Entrypoint::schedule_suspend(void (*suspended)(), void (*resumed)())
{
	_suspended_callback = suspended;
	_resumed_callback   = resumed;

	/*
	 * We always construct the dispatcher when the suspend is scheduled and
	 * destruct it when the suspend is executed.
	 */
	_suspend_dispatcher.construct(*this, *this, &Entrypoint::_handle_suspend);

	/* trigger wakeup of the signal-dispatch loop for suspend */
	Genode::Signal_transmitter(*_suspend_dispatcher).submit();
}


Signal_context_capability Entrypoint::manage(Signal_dispatcher_base &dispatcher)
{
	return _sig_rec->manage(&dispatcher);
}


void Genode::Entrypoint::dissolve(Signal_dispatcher_base &dispatcher)
{
	_sig_rec->dissolve(&dispatcher);
}


namespace {

	struct Constructor
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

			Genode::call_component_construct(env);
		}
	};
}


Entrypoint::Entrypoint(Env &env)
:
	_env(env),
	_rpc_ep(&env.pd(), Component::stack_size(), initial_ep_name())
{
	/* initialize signalling after initializing but before calling the entrypoint */
	init_signal_thread(_env);

	/* initialize emulation of the original synchronous root interface */
	init_root_proxy(_env);

	/*
	 * Invoke Component::construct function in the context of the entrypoint.
	 */
	Constructor_component constructor(env);

	Capability<Constructor> constructor_cap =
		_rpc_ep->manage(&constructor);

	try {
		constructor_cap.call<Constructor::Rpc_construct>();
	} catch (Genode::Blocking_canceled) {
		warning("blocking canceled in entrypoint constructor");
	}

	_rpc_ep->dissolve(&constructor);

	/*
	 * The calling initial thread becomes the signal proxy thread for this
	 * entrypoint
	 */
	_process_incoming_signals();
}


Entrypoint::Entrypoint(Env &env, size_t stack_size, char const *name)
:
	_env(env),
	_rpc_ep(&env.pd(), stack_size, name)
{
	_signal_proxy_thread.construct(env, *this);
}

