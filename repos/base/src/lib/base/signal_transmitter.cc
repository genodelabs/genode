/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2015-03-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/trace/events.h>
#include <base/signal.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;

static Pd_session *_pd;


void Genode::init_signal_transmitter(Env &env) { _pd = &env.pd(); }


void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}

	if (_pd)
		_pd->submit(_context, cnt);
	else
		warning("missing call of 'init_signal_submit'");
}
