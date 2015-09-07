/*
 * \brief  NOVA specific implementation of the signaling framework
 * \author Alexander Boettcher
 * \date   2015-03-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
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

	if (!_context.valid())
		return;

	using namespace Nova;

	uint8_t res = NOVA_OK;
	for (unsigned i = 0; res == NOVA_OK && i < cnt; i++)
		res = sm_ctrl(_context.local_name(), SEMAPHORE_UP);

	if (res != NOVA_OK)
		PDBG("submitting signal failed - error %u - context=0x%lx", res,
		     _context.local_name());
}
