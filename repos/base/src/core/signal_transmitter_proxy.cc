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
#include <signal_source_component.h>
#include <signal_transmitter.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


namespace {

	struct Signal_delivery_proxy
	{
		/*
		 * Wrap pointer into struct to transmit it (core-locally) as plain-old
		 * data.
		 */
		struct Context_ptr { Signal_context_component *ptr; };

		GENODE_RPC(Rpc_deliver, void, _deliver_from_ep, Context_ptr, unsigned);
		GENODE_RPC_INTERFACE(Rpc_deliver);
	};

	struct Signal_delivery_proxy_component
	:
		Rpc_object<Signal_delivery_proxy, Signal_delivery_proxy_component>
	{
		Rpc_entrypoint &_ep;

		Capability<Signal_delivery_proxy> _proxy_cap = _ep.manage(this);

		/**
		 * Constructor
		 *
		 * \param ep  entrypoint to be used as a proxy for delivering signals
		 *            as IPC-reply messages.
		 */
		Signal_delivery_proxy_component(Rpc_entrypoint &ep) : _ep(ep) { }

		/**
		 * Signal_delivery_proxy RPC interface
		 *
		 * This method is executed in the context of the 'ep'. Hence, it
		 * can produce legitimate IPC reply messages to 'Signal_source'
		 * clients.
		 */
		void _deliver_from_ep(Context_ptr context_ptr, unsigned cnt)
		{
			Signal_context_component *context = context_ptr.ptr;
			context->source()->submit(context, cnt);
		}

		/**
		 * Deliver signal via the proxy mechanism
		 *
		 * Since this method perform an RPC call to the 'ep' specified at the
		 * constructor, is must never be called from this ep.
		 *
		 * Called from threads other than 'ep'.
		 */
		void submit(Signal_context_capability cap, unsigned cnt)
		{
			_ep.apply(cap, [&] (Signal_context_component *context) {

				if (context) {
					_proxy_cap.call<Rpc_deliver>(Context_ptr{context}, cnt);
				} else {
					warning("invalid signal-context capability");
				}
			});
		}
	};
}


static Constructible<Signal_delivery_proxy_component> delivery_proxy;


/*
 * Entrypoint that servces the 'Signal_source' RPC objects
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
