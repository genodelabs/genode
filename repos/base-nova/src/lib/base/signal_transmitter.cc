/*
 * \brief  NOVA specific implementation of the signaling framework
 * \author Alexander Boettcher
 * \date   2015-03-16
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/signal.h>
#include <base/log.h>
#include <base/trace/events.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


void Genode::init_signal_transmitter(Env &) { }


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

	if (res == NOVA_OK)
		return;

	warning("submitting signal failed - error ", res, " - context=", _context);

	_context = Signal_context_capability();
}
