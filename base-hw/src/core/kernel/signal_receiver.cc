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


Signal_handler::~Signal_handler()
{
	if (_receiver) { _receiver->remove_handler(this); }
}


Signal_context_killer::~Signal_context_killer()
{
	if (_context) { _context->_killer_destructed(); }
}


Signal_receiver_killer::~Signal_receiver_killer()
{
	if (_receiver) { _receiver->_killer_destructed(); }
}


void Signal_context::_deliverable()
{
	if (!_submits) return;
	_receiver->_add_deliverable(this);
}


Signal_context::~Signal_context() { _receiver->_context_killed(this); }

