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


/**
 * Provide one signal connection per program
 */
static Signal_connection * signal_connection()
{
	static Signal_connection _object;
	return &_object;
}


/************
 ** Signal **
 ************/

void Signal::_dec_ref_and_unlock()
{
	if (_data.context) {
		Lock::Guard lock_guard(_data.context->_lock);
		_data.context->_ref_cnt--;

		/* acknowledge as soon as receipt is fully processed */
		if (_data.context->_ref_cnt == 0) {
			Kernel::ack_signal(_data.context->_cap.dst());
		}
	}
}


void Signal::_inc_ref()
{
	if (_data.context) {
		Lock::Guard lock_guard(_data.context->_lock);
		_data.context->_ref_cnt++;
	}
}


Signal::Signal(Signal::Data data) : _data(data)
{
	if (_data.context) { _data.context->_ref_cnt = 1; }
}


/********************
 ** Signal context **
 ********************/

void Signal_context::submit(unsigned num)
{
	Kernel::submit_signal(_cap.dst(), num);
}


/************************
 ** Signal transmitter **
 ************************/

void Signal_transmitter::submit(unsigned const cnt)
{
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
			env()->parent()->upgrade(s->cap(), "ram_quota=4K");
			session_upgraded = 1;
		}
	}
}


void Signal_receiver::_platform_destructor()
{
	/* release server resources of receiver */
	signal_connection()->free_receiver(_cap);
}


void Signal_receiver::_unsynchronized_dissolve(Signal_context * const c)
{
	/* wait untill all context references disappear and put context to sleep */
	Kernel::kill_signal_context(c->_cap.dst());

	/* release server resources of context */
	signal_connection()->free_context(c->_cap);

	/* reset the context */
	c->_receiver = 0;
	c->_cap = Signal_context_capability();

	/* forget the context */
	_contexts.remove(&c->_receiver_le);
}


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
			c->_cap = s->alloc_context(_cap, (unsigned)c);
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
			env()->parent()->upgrade(s->cap(), "ram_quota=4K");
			session_upgraded = 1;
		}
	}
}


void Signal_receiver::dissolve(Signal_context * const context)
{
	if (context->_receiver != this) { throw Context_not_associated(); }
	Lock::Guard list_lock_guard(_contexts_lock);
	_unsynchronized_dissolve(context);

	/*
	 * We assume that dissolve is always called before the context destructor.
	 * On other platforms a 'context->_destroy_lock' is locked and unlocked at
	 * this point to block until all remaining signals of this context get
	 * destructed and prevent the context from beeing destructed to early.
	 * However on this platform we don't have to wait because
	 * 'kill_signal_context' in '_unsynchronized_dissolve' already does it.
	 */
}


bool Signal_receiver::pending() { return Kernel::signal_pending(_cap.dst()); }


Signal Signal_receiver::wait_for_signal()
{
	/* await a signal */
	if (Kernel::await_signal(_cap.dst(), 0)) {
		PERR("failed to receive signal");
		return Signal(Signal::Data());
	}
	/* get signal data */
	Signal s(*(Signal::Data *)Thread_base::myself()->utcb());
	return s;
}


void Signal_receiver::local_submit(Signal::Data signal)
{
	PERR("not implemented");
}
