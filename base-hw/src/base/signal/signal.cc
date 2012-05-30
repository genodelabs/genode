/*
 * \brief  Implementations of the signaling framework specific for HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/signal.h>
#include <signal_session/connection.h>
#include <kernel/syscalls.h>

using namespace Genode;


/**
 * Provide a static signal connection
 */
static Signal_connection * signal_connection()
{
	static Signal_connection _object;
	return &_object;
}


/*********************
 ** Signal_receiver **
 *********************/


Signal_receiver::Signal_receiver() :
	_cap(signal_connection()->alloc_receiver())
{
	if (!_cap.valid()) {
		PERR("%s: Failed to create receiver", __PRETTY_FUNCTION__);
		while (1) ;
	}
}


Signal_receiver::~Signal_receiver()
{
	/* dissolve all contexts that are managed by us */
	Lock::Guard contexts_guard(_contexts_lock);
	while (1) {
		Signal_context * const c = _contexts.first();
		if (!c) break;
		Lock::Guard context_guard(c->_lock);
		_unsync_dissolve(c);
	}
}


Signal_context_capability Signal_receiver::manage(Signal_context * const c)
{
	/* check if the context is already managed */
	Lock::Guard contexts_guard(_contexts_lock);
	Lock::Guard context_guard(c->_lock);
	if (c->_receiver) throw Context_already_in_use();

	/* create a kernel object that corresponds to the context */
	bool session_upgraded = 0;
	Signal_connection * const s = signal_connection();
	while (1) {
		try {
			c->_cap = s->alloc_context(_cap, (unsigned long)c);
			break;
		} catch (Signal_session::Out_of_metadata)
		{
			/* upgrade session quota and try again, but only once */
			if (session_upgraded) return Signal_context_capability();
			env()->parent()->upgrade(s->cap(), "ram_quota=4K");
			session_upgraded = 1;
		}
	}
	/* assign the context to us */
	c->_receiver = this;
	_contexts.insert(c);
	return c->_cap;
}


Signal Signal_receiver::wait_for_signal()
{
	/* await a signal */
	Kernel::await_signal(_cap.dst());
	Signal s = *(Signal *)Thread_base::myself()->utcb();
	Signal_context * c = s.context();

	/* check if the context of the signal is managed by us */
	Lock::Guard context_guard(c->_lock);
	if (c->_receiver != this) {
		PERR("%s: Context not managed by this receiver", __PRETTY_FUNCTION__);
		while (1) ;
	}
	/* check attributes of the signal and return it */
	if (s.num() == 0) PWRN("Returning signal with num == 0");
	return s;
}


void Signal_receiver::dissolve(Signal_context * const c)
{
	/* check if the context is managed by us */
	Lock::Guard contexts_guard(_contexts_lock);
	Lock::Guard context_guard(c->_lock);
	if (c->_receiver != this) throw Context_not_associated();

	/* unassign the context */
	_unsync_dissolve(c);
}


void Signal_receiver::_unsync_dissolve(Signal_context * const c)
{
	/* reset the context */
	c->_receiver = 0;
	c->_cap = Signal_context_capability();

	/* forget the context */
	_contexts.remove(c);
}

