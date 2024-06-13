/*
 * \brief  Implementations of the signaling framework specific for HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/thread.h>
#include <base/signal.h>
#include <base/env.h>
#include <base/trace/events.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/lock_helper.h>
#include <base/internal/native_utcb.h>
#include <base/internal/native_env.h>
#include <base/internal/capability_space.h>
#include <base/internal/globals.h>

using namespace Genode;

static Env *_env_ptr;
static Env &env()
{
	if (_env_ptr)
		return *_env_ptr;

	class Missing_init_signal_thread { };
	throw Missing_init_signal_thread();
}


/*
 * On base-hw, we don't use a signal thread. We mereely save the PD session
 * pointer of the passed 'env' argument.
 */
void Genode::init_signal_thread(Env &env) { _env_ptr = &env; }


void Genode::init_signal_receiver(Pd_session &, Parent &) { }


Signal_receiver::Signal_receiver() : _pd(env().pd())
{
	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		using Error = Pd_session::Signal_source_error;

		env().pd().signal_source().with_result(
			[&] (Capability<Signal_source> cap) { _cap = cap; },
			[&] (Error e) {
				switch (e) {
				case Error::OUT_OF_RAM:  ram_upgrade = { 2*1024*sizeof(long) }; break;
				case Error::OUT_OF_CAPS: cap_upgrade = { 4 };                   break;
				}
			});

		if (_cap.valid())
			break;

		env().upgrade(Parent::Env::pd(),
		              String<100>("ram_quota=", ram_upgrade, ", "
		                          "cap_quota=", cap_upgrade).string());
	}
}


void Signal_receiver::_platform_destructor()
{
	/* release server resources of receiver */
	env().pd().free_signal_source(_cap);
}


void Signal_receiver::_platform_begin_dissolve(Signal_context &context)
{
	/**
	 * Mark the Signal_context as already pending to prevent the receiver
	 * from taking the mutex, and set an invalid context to prevent further
	 * processing
	 */
	{
		Mutex::Guard context_guard(context._mutex);
		context._pending     = true;
		context._curr_signal = Signal::Data();
	}
	Kernel::kill_signal_context(Capability_space::capid(context._cap));
}


void Signal_receiver::_platform_finish_dissolve(Signal_context &) { }


Signal_context_capability Signal_receiver::manage(Signal_context &context)
{
	/* ensure that the context isn't managed already */
	Mutex::Guard contexts_guard(_contexts_mutex);
	Mutex::Guard context_guard(context._mutex);

	if (context._receiver) {
		error("ill-attempt to manage an already managed signal context");
		return context._cap;
	}

	Signal_receiver &this_receiver = *this;

	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		using Error = Pd_session::Alloc_context_error;

		/* use pointer to signal context as imprint */
		Pd_session::Imprint const imprint { addr_t(&context) };

		_pd.alloc_context(_cap, imprint).with_result(
			[&] (Capability<Signal_context> cap) {
				context._cap      = cap;
				context._receiver = &this_receiver;
				_contexts.insert_as_tail(&context);
			},
			[&] (Error e) {
				switch (e) {
				case Error::OUT_OF_RAM:
					ram_upgrade = Ram_quota { 1024*sizeof(long) };
					break;
				case Error::OUT_OF_CAPS:
					cap_upgrade = Cap_quota { 4 };
					break;
				case Error::INVALID_SIGNAL_SOURCE:
					error("ill-attempt to create context for invalid signal source");
					sleep_forever();
					break;
				}
			});

		if (context._cap.valid())
			return context._cap;

		env().upgrade(Parent::Env::pd(),
		              String<100>("ram_quota=", ram_upgrade, ", "
		                          "cap_quota=", cap_upgrade).string());
	}
}


void Signal_receiver::block_for_signal()
{
	/* wait for a signal */
	if (Kernel::await_signal(Capability_space::capid(_cap))) {
		/* canceled */
		return;
	}

	/* read signal data */
	Signal::Data * const data =
		(Signal::Data *)Thread::myself()->utcb()->data();
	Signal_context * const context = data->context;

	/**
	 * Check for the signal being pending already to prevent a dead-lock
	 * when the context is in destruction, and its mutex is held
	 */
	if (!context->_pending) {
		/* update signal context */
		Mutex::Guard context_guard(context->_mutex);
		unsigned const num    = data->num;
		context->_pending     = true;
		context->_curr_signal = Signal::Data(context, num);
	}

	/* end kernel-aided life-time management */
	Kernel::ack_signal(Capability_space::capid(data->context->_cap));
}


Signal Signal_receiver::pending_signal()
{
	Mutex::Guard contexts_guard(_contexts_mutex);
	Signal::Data result;
	_contexts.for_each_locked([&] (Signal_context &context) {

		if (!context._pending)
			return false;

		_contexts.head(context._next);
		context._pending     = false;
		result               = context._curr_signal;
		context._curr_signal = Signal::Data();

		Trace::Signal_received trace_event(context, result.num);
		return true;
	});
	if (result.context) {
		Mutex::Guard context_guard(result.context->_mutex);
		if (result.num == 0)
			warning("returning signal with num == 0");

		return result;
	}

	/* look for pending signals */
	if (Kernel::pending_signal(Capability_space::capid(_cap)) != 0)
		return Signal();

	/* read signal data */
	Signal::Data * const data =
		(Signal::Data *)Thread::myself()->utcb()->data();
	Signal_context * const context = data->context;

	{
		/* update signal context */
		Mutex::Guard context_guard(context->_mutex);
		context->_pending     = false;
		context->_curr_signal = Signal::Data(context, data->num);
		result                = context->_curr_signal;
	}

	/* end kernel-aided life-time management */
	Kernel::ack_signal(Capability_space::capid(data->context->_cap));
	return result;
}


void Signal_receiver::unblock_signal_waiter(Rpc_entrypoint &rpc_ep)
{
	Kernel::cancel_next_await_signal(native_thread_id(&rpc_ep));
}


void Signal_receiver::local_submit(Signal::Data) { Genode::error("not implemented"); }
