/*
 * \brief   Kernel backend for asynchronous inter-process communication
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/signal_receiver.h>

using namespace Kernel;


void Signal_handler::_cancel_waiting()
{
	if (_receiver) { _receiver->_handler_cancelled(this); }
}


void Signal_context_killer::_cancel_waiting()
{
	if (_context) { _context->_killer_cancelled(); }
}


void Signal_receiver_killer::_cancel_waiting()
{
	if (_receiver) { _receiver->_killer_cancelled(); }
}


void Signal_context::_deliverable()
{
	if (!_submits) return;
	_receiver->_add_deliverable(this);
}


Signal_context::~Signal_context() { _receiver->_context_killed(this); }


Signal_context::Signal_context(Signal_receiver * const r, unsigned const imprint)
:
	_deliver_fe(this), _contexts_fe(this), _receiver(r),
	_imprint(imprint), _submits(0), _ack(1), _kill(0), _killer(0)
{
	if (r->_add_context(this)) { throw Assign_to_receiver_failed(); }
}
