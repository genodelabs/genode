/*
 * \brief  Signal handler thread implementation
 * \author Christian Prochaska
 * \date   2011-08-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>

/* libc includes */
#include <unistd.h>

/* GDB monitor includes */
#include "thread_info.h"
#include "signal_handler_thread.h"

using namespace Genode;
using namespace Gdb_monitor;

static bool const verbose = false;

Signal_handler_thread::Signal_handler_thread(Signal_receiver *receiver)
:
	Thread<2*4096>("sig_handler"),
	_signal_receiver(receiver)
{
	if (pipe(_pipefd))
		PERR("could not create pipe");

}


void Signal_handler_thread::entry()
{
	while(1) {
		Signal s = _signal_receiver->wait_for_signal();

		if (verbose)
			PDBG("received exception signal");

		/* default is segmentation fault */
		unsigned long sig = 0;

		if (Thread_info *thread_info = dynamic_cast<Thread_info*>(s.context()))
			/* thread trapped */
			sig = thread_info->lwpid();

		write(_pipefd[1], &sig, sizeof(sig));
	}

	sleep_forever();
};
