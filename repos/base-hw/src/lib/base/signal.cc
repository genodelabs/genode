/*
 * \brief  Implementations of the signaling framework specific for HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/thread.h>
#include <base/signal.h>
#include <base/env.h>
#include <base/trace/events.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/native_env.h>
#include <base/internal/capability_space.h>
#include <base/internal/globals.h>

using namespace Genode;


namespace Genode {

	/*
	 * On base-hw, no signal thread is needed.
	 */
	void init_signal_thread(Env &) __attribute__((weak));
	void init_signal_thread(Env &) { }
	void destroy_signal_thread() { }
}


/********************
 ** Signal context **
 ********************/

void Signal_context::submit(unsigned) { Genode::error("not implemented"); }


/************************
 ** Signal transmitter **
 ************************/

void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	Kernel::submit_signal(Capability_space::capid(_context), cnt);
}


/*********************
 ** Signal_receiver **
 *********************/

Signal_receiver::Signal_receiver()
{
	retry<Pd_session::Out_of_metadata>(
		[&] () { _cap = internal_env().pd().alloc_signal_source(); },
		[&] () {
			log("upgrading quota donation for PD session");
			internal_env().upgrade(Parent::Env::pd(), "ram_quota=8K");
		}
	);
}


void Signal_receiver::_platform_destructor()
{
	/* release server resources of receiver */
	env()->pd_session()->free_signal_source(_cap);
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

	retry<Pd_session::Out_of_metadata>(
		[&] () {
			/* use signal context as imprint */
			c->_cap = env()->pd_session()->alloc_context(_cap, (unsigned long)c);
			c->_receiver = this;
			_contexts.insert(&c->_receiver_le);
			return c->_cap;
		},
		[&] () { upgrade_pd_quota_non_blocking(1024 * sizeof(addr_t)); });

	return c->_cap;
}


void Signal_receiver::block_for_signal()
{
	/* wait for a signal */
	if (Kernel::await_signal(Capability_space::capid(_cap))) {
		Genode::error("failed to receive signal");
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


void Signal_receiver::local_submit(Signal::Data) { Genode::error("not implemented"); }
