/*
 * \brief  Implementations of the signaling framework specific for HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <signal_session/connection.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;

/************************
 ** Signal transmitter **
 ************************/
void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	Kernel::submit_signal(_context.dst(), cnt);
}
