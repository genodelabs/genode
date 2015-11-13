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
#include <base/thread.h>
#include <base/signal.h>
#include <signal_session/connection.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

/********************
 ** Signal context **
 ********************/

void Signal_context::submit(unsigned) { PERR("not implemented"); }

/************************
 ** Signal transmitter **
 ************************/

void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	Kernel::submit_signal(_context.dst(), cnt);
}


/*********************
 ** Signal_receiver **
 *********************/

Signal_receiver::Signal_receiver()
{
	/* create a kernel object that corresponds to the receiver */
	bool session_upgraded = 0;
	Signal_connection * const s = signal_connection();
	while (1) {
		try {
			_cap = s->alloc_receiver();
			return;
		} catch (Signal_session::Out_of_metadata)
		{
			/* upgrade session quota and try again, but only once */
			if (session_upgraded) {
				PERR("failed to alloc signal receiver");
				_cap = Signal_receiver_capability();
				return;
			}
			PINF("upgrading quota donation for SIGNAL session");
			env()->parent()->upgrade(s->cap(), "ram_quota=8K");
			session_upgraded = 1;
		}
	}
}


void Signal_receiver::_platform_destructor()
{
	/* release server resources of receiver */
	signal_connection()->free_receiver(_cap);
}


void Signal_receiver::_platform_begin_dissolve(Signal_context * const c)
{
	Kernel::kill_signal_context(c->_cap.dst());
}

void Signal_receiver::_platform_finish_dissolve(Signal_context *) { }


Signal_context_capability Signal_receiver::manage(Signal_context * const c)
{
	/* ensure that the context isn't managed already */
	Lock::Guard contexts_guard(_contexts_lock);
	Lock::Guard context_guard(c->_lock);
	if (c->_receiver) { throw Context_already_in_use(); }

	/* create a context kernel-object at the receiver kernel-object */
	bool session_upgraded = 0;
	Signal_connection * const s = signal_connection();
	while (1) {
		try {
			c->_cap = s->alloc_context(_cap, (unsigned long)c);
			c->_receiver = this;
			_contexts.insert(&c->_receiver_le);
			return c->_cap;
		} catch (Signal_session::Out_of_metadata)
		{
			/* upgrade session quota and try again, but only once */
			if (session_upgraded) {
				PERR("failed to alloc signal context");
				return Signal_context_capability();
			}
			PINF("upgrading quota donation for signal session");
			env()->parent()->upgrade(s->cap(), "ram_quota=8K");
			session_upgraded = 1;
		}
	}
}


void Signal_receiver::block_for_signal()
{
	/* wait for a signal */
	if (Kernel::await_signal(_cap.dst())) {
		PERR("failed to receive signal");
		return;
	}
	/* read signal data */
	const void * const     utcb    = Thread_base::myself()->utcb()->base();
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
	Kernel::ack_signal(data->context->_cap.dst());
}


void Signal_receiver::local_submit(Signal::Data) { PERR("not implemented"); }
