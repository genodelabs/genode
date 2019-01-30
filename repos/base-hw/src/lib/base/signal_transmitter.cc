/*
 * \brief  Implementations of the signaling framework specific for HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/signal.h>
#include <base/trace/events.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/native_env.h>
#include <base/internal/capability_space.h>
#include <base/internal/globals.h>

using namespace Genode;

void Genode::init_signal_transmitter(Env &) { }


void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	Kernel::submit_signal(Capability_space::capid(_context), cnt);
}
