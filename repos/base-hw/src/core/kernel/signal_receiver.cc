/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/signal_receiver.h>

using namespace Kernel;


/************************
 ** Signal_ack_handler **
 ************************/

Signal_ack_handler::~Signal_ack_handler()
{
	if (_signal_context) { _signal_context->ack_handler(0); }
}


/********************
 ** Signal_handler **
 ********************/

void Signal_handler::cancel_waiting()
{
	if (_receiver) {
		_receiver->_handler_cancelled(this);
		_receiver = 0;
	}
}


Signal_handler::Signal_handler()
: _handlers_fe(this), _receiver(0) { }


Signal_handler::~Signal_handler() { cancel_waiting(); }


/***************************
 ** Signal_context_killer **
 ***************************/

void Signal_context_killer::cancel_waiting()
{
	if (_context) { _context->_killer_cancelled(); }
}


Signal_context_killer::Signal_context_killer() : _context(0) { }


Signal_context_killer::~Signal_context_killer() { cancel_waiting(); }


/********************
 ** Signal_context **
 ********************/

void Signal_context::_deliverable()
{
	if (_submits) { _receiver->_add_deliverable(this); }
}


void Signal_context::_delivered()
{
	_submits = 0;
	_ack     = 0;
}


void Signal_context::_killer_cancelled() { _killer = 0; }


void Signal_context::ack_handler(Signal_ack_handler * const h)
{
	_ack_handler = h ? h : &_default_ack_handler;
	_ack_handler->_signal_context = this;
}


int Signal_context::submit(unsigned const n)
{
	if (_killed || _submits >= (unsigned)~0 - n) { return -1; }
	_submits += n;
	if (_ack) { _deliverable(); }
	return 0;
}


void Signal_context::ack()
{
	_ack_handler->_signal_acknowledged();
	if (_ack) { return; }
	if (!_killed) {
		_ack = 1;
		_deliverable();
		return;
	}
	if (_killer) {
		_killer->_context = 0;
		_killer->_signal_context_kill_done();
		_killer = 0;
	}
}


int Signal_context::kill(Signal_context_killer * const k)
{
	/* check if in a kill operation or already killed */
	if (_killed) {
		if (_ack) { return 0; }
		return -1;
	}
	/* kill directly if there is no unacknowledged delivery */
	if (_ack) {
		_killed = 1;
		return 0;
	}
	/* wait for delivery acknowledgement */
	_killer = k;
	_killed = 1;
	_killer->_context = this;
	_killer->_signal_context_kill_pending();
	return 0;
}


Signal_context::~Signal_context()
{
	if (_killer) { _killer->_signal_context_kill_failed(); }
	_receiver->_context_destructed(this);
}


Signal_context::Signal_context(Signal_receiver * const r, unsigned const imprint)
:
	_deliver_fe(this),
	_contexts_fe(this),
	_receiver(r),
	_imprint(imprint),
	_submits(0),
	_ack(1),
	_killed(0),
	_killer(0),
	_ack_handler(&_default_ack_handler)
{
	r->_add_context(this);
}


/*********************
 ** Signal_receiver **
 *********************/

void Signal_receiver::_add_deliverable(Signal_context * const c)
{
	if (!c->_deliver_fe.enqueued()) {
		_deliver.enqueue(&c->_deliver_fe);
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
		auto const context = _deliver.dequeue()->object();
		auto const imprint =
			reinterpret_cast<Signal_imprint>(context->_imprint);
		Signal::Data data(imprint, context->_submits);

		/* communicate signal data to handler */
		auto const handler = _handlers.dequeue()->object();
		handler->_receiver = 0;
		handler->_receive_signal(&data, sizeof(data));
		context->_delivered();
	}
}


void Signal_receiver::_context_destructed(Signal_context * const c)
{
	_contexts.remove(&c->_contexts_fe);
	if (!c->_deliver_fe.enqueued()) { return; }
	_deliver.remove(&c->_deliver_fe);
}


void Signal_receiver::_handler_cancelled(Signal_handler * const h) {
	_handlers.remove(&h->_handlers_fe); }


void Signal_receiver::_add_context(Signal_context * const c) {
	_contexts.enqueue(&c->_contexts_fe); }


int Signal_receiver::add_handler(Signal_handler * const h)
{
	if (h->_receiver) { return -1; }
	_handlers.enqueue(&h->_handlers_fe);
	h->_receiver = this;
	h->_await_signal(this);
	_listen();
	return 0;
}


Signal_receiver::~Signal_receiver()
{
	/* destruct all attached contexts */
	while (Signal_context * c = _contexts.dequeue()->object()) {
		c->~Signal_context();
	}
}
