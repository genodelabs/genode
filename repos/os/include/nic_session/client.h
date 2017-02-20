/*
 * \brief  Client-side NIC session interface
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC_SESSION__CLIENT_H_
#define _INCLUDE__NIC_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <nic_session/capability.h>
#include <packet_stream_tx/client.h>
#include <packet_stream_rx/client.h>

namespace Nic { class Session_client; }


class Nic::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Packet_stream_tx::Client<Tx> _tx;
		Packet_stream_rx::Client<Rx> _rx;

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 */
		Session_client(Session_capability       session,
		               Genode::Range_allocator &tx_buffer_alloc,
		               Genode::Region_map      &rm)
		:
			Genode::Rpc_client<Session>(session),
			_tx(call<Rpc_tx_cap>(), rm, tx_buffer_alloc),
			_rx(call<Rpc_rx_cap>(),rm )
		{ }


		/***************************
		 ** NIC session interface **
		 ***************************/

		Mac_address mac_address() override { return call<Rpc_mac_address>(); }

		Tx *tx_channel() { return &_tx; }
		Rx *rx_channel() { return &_rx; }
		Tx::Source *tx() { return _tx.source(); }
		Rx::Sink   *rx() { return _rx.sink(); }

		void link_state_sigh(Genode::Signal_context_capability sigh) override
		{
			call<Rpc_link_state_sigh>(sigh);
		}

		bool link_state() override { return call<Rpc_link_state>(); }
};

#endif /* _INCLUDE__NIC_SESSION__CLIENT_H_ */
