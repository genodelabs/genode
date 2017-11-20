/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/rpc_server.h>
#include <base/trace/events.h>

/* core-local includes */
#include <core_env.h>
#include <signal_source_component.h>
#include <signal_transmitter.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


static Constructible<Signal_delivery_proxy_component> delivery_proxy;


/*
 * Entrypoint that serves the 'Signal_source' RPC objects
 */
static Rpc_entrypoint *_ep;


void Genode::init_core_signal_transmitter(Rpc_entrypoint &ep) { _ep = &ep; }


void Genode::init_signal_transmitter(Env &)
{
	if (!delivery_proxy.constructed() && _ep)
		delivery_proxy.construct(*_ep);
}


void Signal_transmitter::submit(unsigned cnt)
{
	{
		Trace::Signal_submit trace_event(cnt);
	}
	delivery_proxy->submit(_context, cnt);
}


Rpc_entrypoint &Core_env::signal_ep()
{
	static Rpc_entrypoint ep(nullptr, ENTRYPOINT_STACK_SIZE,
	                         "signal_entrypoint");
	return ep;
}
