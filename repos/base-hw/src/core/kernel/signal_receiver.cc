/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/signal_receiver.h>
#include <kernel/thread.h>

using namespace Kernel;


/********************
 ** Signal_handler **
 ********************/

void Signal_handler::cancel_waiting()
{
	if (_receiver) {
		_receiver->_handler_cancelled(*this);
		_receiver = 0;
	}
}


Signal_handler::Signal_handler(Thread &thread) : _thread { thread } { }

Signal_handler::~Signal_handler() { cancel_waiting(); }


/***************************
 ** Signal_context_killer **
 ***************************/

void Signal_context_killer::cancel_waiting()
{
	if (_context) { _context->_killer_cancelled(); }
}


Signal_context_killer::Signal_context_killer(Thread &thread)
:
	_thread { thread }
{ }


Signal_context_killer::~Signal_context_killer() { cancel_waiting(); }


/********************
 ** Signal_context **
 ********************/

void Signal_context::_deliverable()
{
	if (_submits) { _receiver._add_deliverable(*this); }
}


void Signal_context::_delivered()
{
	_submits = 0;
	_ack     = 0;
}


void Signal_context::_killer_cancelled() { _killer = 0; }


bool Signal_context::can_submit(unsigned const n) const
{
	if (_killed || _submits >= (unsigned)~0 - n) { return false; }
	return true;
}


void Signal_context::submit(unsigned const n)
{
	if (_killed || _submits >= (unsigned)~0 - n) { return; }
	_submits += n;
	if (_ack) { _deliverable(); }
}


void Signal_context::ack()
{
	if (_ack) { return; }
	if (!_killed) {
		_ack = 1;
		_deliverable();
		return;
	}
	if (_killer) {
		_killer->_context = 0;
		_killer->_thread.signal_context_kill_done();
		_killer = 0;
	}
}


bool Signal_context::can_kill() const
{
	/* check if in a kill operation or already killed */
	if (_killed) {
		if (_ack) { return true; }
		return false;
	}
	return true;
}


void Signal_context::kill(Signal_context_killer &k)
{
	/* check if in a kill operation or already killed */
	if (_killed) {
		return;
	}
	/* kill directly if there is no unacknowledged delivery */
	if (_ack) {
		_killed = 1;
		return;
	}
	/* wait for delivery acknowledgement */
	_killer = &k;
	_killed = 1;
	_killer->_context = this;
	_killer->_thread.signal_context_kill_pending();
}


Signal_context::~Signal_context()
{
	if (_killer) { _killer->_thread.signal_context_kill_failed(); }
	_receiver._context_destructed(*this);
}


Signal_context::Signal_context(Signal_receiver & r, addr_t const imprint)
: _receiver(r),
  _imprint(imprint)
{
	r._add_context(*this);
}


/*********************
 ** Signal_receiver **
 *********************/

void Signal_receiver::_add_deliverable(Signal_context &c)
{
	if (!c._deliver_fe.enqueued()) {
		_deliver.enqueue(c._deliver_fe);
	}
	_listen();
}


void Signal_receiver::_listen()
{
	while (1)
	{
		/* check for deliverable signals and waiting handlers */
		if (_deliver.empty() || _handlers.empty()) { return; }

		/* create a signal data-object */
		typedef Genode::Signal_context * Signal_imprint;

		_deliver.dequeue([&] (Signal_context::Fifo_element &elem) {
			auto const context = &elem.object();

			Signal_imprint const imprint =
				reinterpret_cast<Signal_imprint>(context->_imprint);
			Signal::Data data(imprint, context->_submits);

			/* communicate signal data to handler */
			_handlers.dequeue([&] (Signal_handler::Fifo_element &elem) {
				auto const handler = &elem.object();
				handler->_receiver = nullptr;
				handler->_thread.signal_receive_signal(&data, sizeof(data));
			});
			context->_delivered();
		});
	}
}


void Signal_receiver::_context_destructed(Signal_context &c)
{
	_contexts.remove(c._contexts_fe);
	if (!c._deliver_fe.enqueued()) { return; }
	_deliver.remove(c._deliver_fe);
}


void Signal_receiver::_handler_cancelled(Signal_handler &h) {
	_handlers.remove(h._handlers_fe); }


void Signal_receiver::_add_context(Signal_context &c) {
	_contexts.enqueue(c._contexts_fe); }


bool Signal_receiver::can_add_handler(Signal_handler const &h) const
{
	if (h._receiver) { return false; }
	return true;
}


void Signal_receiver::add_handler(Signal_handler &h)
{
	if (h._receiver) { return; }
	_handlers.enqueue(h._handlers_fe);
	h._receiver = this;
	h._thread.signal_wait_for_signal();
	_listen();
}


Signal_receiver::~Signal_receiver()
{
	/* destruct all attached contexts */
	_contexts.dequeue_all([] (Signal_context::Fifo_element &elem) {
		elem.object().~Signal_context(); });
}
