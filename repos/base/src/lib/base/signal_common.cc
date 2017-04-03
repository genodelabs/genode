/*
 * \brief  Platform-independent part of signal framework
 * \author Norman Feske
 * \author Christian Prochaska
 * \author Martin Stein
 * \date   2013-02-21
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/signal.h>
#include <base/trace/events.h>

using namespace Genode;


/************
 ** Signal **
 ************/

Signal::Signal(Signal const &other)
{
	_data.context = other._data.context;
	_data.num     = other._data.num;

	_inc_ref();
}


Signal & Signal::operator=(Signal const &other)
{
	if ((_data.context == other._data.context) &&
	    (_data.num     == other._data.num))
		return *this;

	_dec_ref_and_unlock();

	_data.context = other._data.context;
	_data.num     = other._data.num;

	_inc_ref();

	return *this;
}


Signal::~Signal() { _dec_ref_and_unlock(); }


void Signal::_dec_ref_and_unlock()
{
	if (_data.context) {
		Lock::Guard lock_guard(_data.context->_lock);
		_data.context->_ref_cnt--;
		if (_data.context->_ref_cnt == 0)
			_data.context->_destroy_lock.unlock();
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
	if (_data.context) {
		_data.context->_ref_cnt++;

		/*
		 * Defer the destruction of the context until the handling of the
		 * 'Signal' has completed.
		 *
		 * Normally, the context can only have one 'Signal' in flight, which is
		 * destroyed before 'pending_signal' is called the next time. However,
		 * one exception is a signal handler that unexpectedly calls
		 * 'pending_signal' itself (i.e., via 'wait_and_dispatch_one_io_signal').
		 * As this is dangerous programming pattern (that should be fixed), we
		 * print a warning.
		 *
		 * In this situation, the context-destroy lock is already taken by the
		 * outer scope. To avoid a deadlock during the attempt to create
		 * a second 'Signal' object for the same context, we take the lock
		 * only in the outer scope (where the context's reference counter
		 * is in its clear state).
		 */
		if (_data.context->_ref_cnt == 1) {
			_data.context->_destroy_lock.lock();
		} else {

			/* print warning only once to avoid flooding the log */
			static bool printed;
			if (!printed) {
				warning("attempt to handle the same signal context twice (nested)");
				printed = true;
			}
		}
	}
}


/********************
 ** Signal_context **
 ********************/

Signal_context::~Signal_context()
{
	/*
	 * Detect bug in an application where a signal context is destroyed prior
	 * dissolving it from the signal receiver.
	 */
	if (_receiver)
		error("Destructing undissolved signal context");
}


/************************
 ** Signal_transmitter **
 ************************/

Signal_transmitter::Signal_transmitter(Signal_context_capability context)
: _context(context) { }


void Signal_transmitter::context(Signal_context_capability context) {
	_context = context; }

Signal_context_capability Signal_transmitter::context() { return _context; }


/*********************
 ** Signal_receiver **
 *********************/

Signal Signal_receiver::wait_for_signal()
{
	for (;;) {

		try {
			return pending_signal();
		} catch (Signal_not_pending) { }

		/* block until the receiver has received a signal */
		block_for_signal();
	}
}


Signal Signal_receiver::pending_signal()
{
	Lock::Guard list_lock_guard(_contexts_lock);

	/* look up the contexts for the pending signal */
	for (List_element<Signal_context> *le = _contexts.first(); le; le = le->next()) {

		Signal_context *context = le->object();

		Lock::Guard lock_guard(context->_lock);

		/* check if context has a pending signal */
		if (!context->_pending)
			continue;

		context->_pending = false;
		Signal::Data result = context->_curr_signal;

		/* invalidate current signal in context */
		context->_curr_signal = Signal::Data(0, 0);

		if (result.num == 0)
			warning("returning signal with num == 0");

		Trace::Signal_received trace_event(*context, result.num);

		/* return last received signal */
		return result;
	}

	/*
	 * Normally, we should never arrive at this point because that would
	 * mean, the '_signal_available' semaphore was increased without
	 * registering the signal in any context associated to the receiver.
	 *
	 * However, if a context gets dissolved right after submitting a
	 * signal, we may have increased the semaphore already. In this case
	 * the signal-causing context is absent from the list.
	 */
	throw Signal_not_pending();
}


Signal_receiver::~Signal_receiver()
{
	Lock::Guard list_lock_guard(_contexts_lock);

	/* disassociate contexts from the receiver */
	for (List_element<Signal_context> *le; (le = _contexts.first()); )
		_unsynchronized_dissolve(le->object());

	_platform_destructor();
}


void Signal_receiver::_unsynchronized_dissolve(Signal_context * const context)
{
	_platform_begin_dissolve(context);

	/* tell core to stop sending signals referring to the context */
	env_deprecated()->pd_session()->free_context(context->_cap);

	/* restore default initialization of signal context */
	context->_receiver = nullptr;
	context->_cap      = Signal_context_capability();

	/* remove context from context list */
	_contexts.remove(&context->_receiver_le);

	_platform_finish_dissolve(context);
}


void Signal_receiver::dissolve(Signal_context *context)
{
	if (context->_receiver != this)
		throw Context_not_associated();

	Lock::Guard list_lock_guard(_contexts_lock);

	_unsynchronized_dissolve(context);

	Lock::Guard context_destroy_lock_guard(context->_destroy_lock);
}



bool Signal_receiver::pending()
{
	Lock::Guard list_lock_guard(_contexts_lock);

	/* look up the contexts for the pending signal */
	for (List_element<Signal_context> *le = _contexts.first(); le; le = le->next()) {

		Signal_context *context = le->object();

		Lock::Guard lock_guard(context->_lock);

		if (context->_pending)
			return true;
	}
	return false;
}
