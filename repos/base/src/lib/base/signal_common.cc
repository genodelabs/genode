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
:
	_data(other._data.context, other._data.num)
{
	_inc_ref();
}


Signal & Signal::operator=(Signal const &other)
{
	bool const same_context = (_data.context == other._data.context);

	/* don't change ref cnt if it's the same context */
	if (!same_context)
		_dec_ref_and_unlock();

	_data.context = other._data.context;
	_data.num     = other._data.num;

	if (!same_context)
		_inc_ref();

	return *this;
}


Signal::~Signal() { _dec_ref_and_unlock(); }


void Signal::_dec_ref_and_unlock()
{
	if (_data.context) {
		Mutex::Guard context_guard(_data.context->_mutex);
		_data.context->_ref_cnt--;
		if (_data.context->_ref_cnt == 0)
			_data.context->_destroy_mutex.release();
	}
}


void Signal::_inc_ref()
{
	if (_data.context) {
		Mutex::Guard context_guard(_data.context->_mutex);
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
			_data.context->_destroy_mutex.acquire();
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

		Signal sig = pending_signal();

		if (sig.valid())
			return sig;

		/* block until the receiver has received a signal */
		block_for_signal();
	}
}


Signal_receiver::~Signal_receiver()
{
	Mutex::Guard contexts_guard(_contexts_mutex);

	/* disassociate contexts from the receiver */
	while (Signal_context *context = _contexts.head()) {
		_platform_begin_dissolve(context);
		_unsynchronized_dissolve(context);
		_platform_finish_dissolve(context);
	}

	_platform_destructor();
}


void Signal_receiver::_unsynchronized_dissolve(Signal_context * const context)
{
	/* tell core to stop sending signals referring to the context */
	_pd.free_context(context->_cap);

	/* restore default initialization of signal context */
	context->_receiver = nullptr;
	context->_cap      = Signal_context_capability();

	/* remove context from context list */
	_contexts.remove(context);
}


void Signal_receiver::dissolve(Signal_context *context)
{
	if (context->_receiver != this)
		throw Context_not_associated();

	{
		/*
		 * We must adhere to the following lock-taking order:
		 *
		 * 1. Taking the lock for the list of contexts ('_contexts_mutex')
		 * 2. Taking the context-registry lock (this happens inside
		 *    '_platform_begin_dissolve' on platforms that use such a
		 *    registry)
		 * 3. Taking the lock for an individual signal context
		 */
		Mutex::Guard contexts_guard(_contexts_mutex);

		_platform_begin_dissolve(context);

		Mutex::Guard context_guard(context->_mutex);

		_unsynchronized_dissolve(context);
	}

	_platform_finish_dissolve(context);

	Mutex::Guard context_destroy_guard(context->_destroy_mutex);
}


void Signal_receiver::Context_ring::insert_as_tail(Signal_context *re)
{
	if (_head) {
		re->_prev = _head->_prev;
		re->_next = _head;
		_head->_prev = _head->_prev->_next = re;
	} else {
		_head = re->_prev = re->_next = re;
	}
}


void Signal_receiver::Context_ring::remove(Signal_context const *re)
{
	if (re->_next != re) {
		if (re == _head) { _head = re->_next; }
		re->_prev->_next = re->_next;
		re->_next->_prev = re->_prev;
	}
	else { _head = nullptr; }
}
