/*
 * \brief  Mechanism to deliver signals via core
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SIGNAL_DELIVERY_PROXY_H_
#define _CORE__INCLUDE__SIGNAL_DELIVERY_PROXY_H_

namespace Genode {

	struct Signal_delivery_proxy
	{
		GENODE_RPC(Rpc_deliver, void, _deliver_from_ep, Signal_context_capability, unsigned);
		GENODE_RPC_INTERFACE(Rpc_deliver);
	};

	struct Signal_delivery_proxy_component
	:
		Rpc_object<Signal_delivery_proxy, Signal_delivery_proxy_component>
	{
		Rpc_entrypoint &_ep;

		Capability<Signal_delivery_proxy> _proxy_cap;

		/**
		 * Constructor
		 *
		 * \param ep  entrypoint to be used as a proxy for delivering signals
		 *            as IPC-reply messages.
		 */
		Signal_delivery_proxy_component(Rpc_entrypoint &ep) : _ep(ep)
		{
			_proxy_cap = _ep.manage(this);
		}

		~Signal_delivery_proxy_component()
		{
			if (_proxy_cap.valid())
				_ep.dissolve(this);
		}

		/**
		 * Signal_delivery_proxy RPC interface
		 *
		 * This method is executed in the context of the 'ep'. Hence, it
		 * can produce legitimate IPC reply messages to 'Signal_source'
		 * clients.
		 */
		void _deliver_from_ep(Signal_context_capability cap, unsigned cnt)
		{
			_ep.apply(cap, [&] (Signal_context_component *context) {
				if (context)
					context->source()->submit(context, cnt);
				else
					warning("invalid signal-context capability");
			});
		}

		/**
		 * Deliver signal via the proxy mechanism
		 *
		 * Since this method perform an RPC call to the 'ep' specified at the
		 * constructor, is must never be called from this ep.
		 *
		 * Called from threads other than 'ep'.
		 */
		void submit(Signal_context_capability cap, unsigned cnt) {
			_proxy_cap.call<Rpc_deliver>(cap, cnt); }
	};
}

#endif /* _CORE__INCLUDE__SIGNLA_DELIVERY_PROXY_H_ */
