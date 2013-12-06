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

void Signal_handler::_cancel_waiting()
{
	if (_receiver) { _receiver->_handler_cancelled(this); }
}


/***************************
 ** Signal_context_killer **
 ***************************/

void Signal_context_killer::_cancel_waiting()
{
	if (_context) { _context->_killer_cancelled(); }
}


/********************
 ** Signal_context **
 ********************/

void Signal_context::_deliverable()
{
	if (_submits) { _receiver->_add_deliverable(this); }
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
