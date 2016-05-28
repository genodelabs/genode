/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2008-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#include <base/env.h>
#include <base/trace/events.h>

using namespace Genode;


/************************
 ** Signal transmitter **
 ************************/

void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	env()->pd_session()->submit(_context, cnt);
}
