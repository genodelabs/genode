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

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/lock_helper.h>
#include <base/internal/native_utcb.h>
#include <base/internal/native_env.h>
#include <base/internal/capability_space.h>
#include <base/internal/globals.h>

using namespace Genode;

static Pd_session *_pd_ptr;
static Pd_session &pd()
{
	if (_pd_ptr)
		return *_pd_ptr;

	class Missing_init_signal_thread { };
	throw Missing_init_signal_thread();
}


/*
 * On base-hw, we don't use a signal thread. We mereely save the PD session
 * pointer of the passed 'env' argument.
 */
void Genode::init_signal_thread(Env &env) { _pd_ptr = &env.pd(); }
void Genode::destroy_signal_thread() { }


Signal_receiver::Signal_receiver()
{
	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		try {
			_cap = pd().alloc_signal_source();
			break;
		}
		catch (Out_of_ram)  { ram_upgrade = Ram_quota { 2*1024*sizeof(long) }; }
		catch (Out_of_caps) { cap_upgrade = Cap_quota { 4 }; }

		internal_env().upgrade(Parent::Env::pd(),
		                       String<100>("ram_quota=", ram_upgrade, ", "
		                                   "cap_quota=", cap_upgrade).string());
	}
}


void Signal_receiver::_platform_destructor()
{
	/* release server resources of receiver */
	pd().free_signal_source(_cap);
}


void Signal_receiver::_platform_begin_dissolve(Signal_context * const c)
{
	Kernel::kill_signal_context(Capability_space::capid(c->_cap));
}

void Signal_receiver::_platform_finish_dissolve(Signal_context *) { }


Signal_context_capability Signal_receiver::manage(Signal_context * const c)
{
	/* ensure that the context isn't managed already */
	Lock::Guard contexts_guard(_contexts_lock);
	Lock::Guard context_guard(c->_lock);
	if (c->_receiver) { throw Context_already_in_use(); }

	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		try {
			/* use signal context as imprint */
			c->_cap = pd().alloc_context(_cap, (unsigned long)c);
			c->_receiver = this;
			_contexts.insert_as_tail(c);
			return c->_cap;
		}
		catch (Out_of_ram)  { ram_upgrade = Ram_quota { 1024*sizeof(long) }; }
		catch (Out_of_caps) { cap_upgrade = Cap_quota { 4 }; }

		internal_env().upgrade(Parent::Env::pd(),
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
	const void * const     utcb    = Thread::myself()->utcb()->data();
	Signal::Data * const   data    = (Signal::Data *)utcb;
	Signal_context * const context = data->context;
	{
		/* update signal context */
		Lock::Guard lock_guard(context->_lock);
		unsigned const num    = context->_curr_signal.num + data->num;
		context->_pending     = true;
		context->_curr_signal = Signal::Data(context, num);
	}
	/* end kernel-aided life-time management */
	Kernel::ack_signal(Capability_space::capid(data->context->_cap));
}


void Signal_receiver::unblock_signal_waiter(Rpc_entrypoint &rpc_ep)
{
	Kernel::cancel_next_await_signal(native_thread_id(&rpc_ep));
}


void Signal_receiver::local_submit(Signal::Data) { Genode::error("not implemented"); }
