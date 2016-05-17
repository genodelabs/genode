/*
 * \brief  Signal handler thread implementation
 * \author Christian Prochaska
 * \date   2011-08-15
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "signal_handler_thread.h"

using namespace Genode;
using namespace Gdb_monitor;

Signal_handler_thread::Signal_handler_thread(Signal_receiver *receiver)
:
	Thread_deprecated<SIGNAL_HANDLER_THREAD_STACK_SIZE>("sig_handler"),
	_signal_receiver(receiver) { }


void Signal_handler_thread::entry()
{
	while(1) {
		Signal s = _signal_receiver->wait_for_signal();

		Signal_dispatcher_base *signal_dispatcher =
		    dynamic_cast<Signal_dispatcher_base*>(s.context());

		signal_dispatcher->dispatch(s.num());
	}
};
