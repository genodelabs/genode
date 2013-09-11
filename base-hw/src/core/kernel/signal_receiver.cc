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


void Kernel::Signal_context::_deliverable()
{
	if (!_submits) return;
	_receiver->_add_deliverable(this);
}


Kernel::Signal_context::~Signal_context() { _receiver->_context_killed(this); }
