/*
 * \brief  Generic implementation parts of the signaling framework which are
 *         implemented platform specifically, e.g. base-hw and base-nova.
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#include <signal_session/connection.h>

using namespace Genode;

/************************
 ** Signal transmitter **
 ************************/
void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	connection()->submit(_context, cnt);
}
