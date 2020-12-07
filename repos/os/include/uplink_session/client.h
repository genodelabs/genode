/*
 * \brief  Client-side Uplink session interface
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_SESSION__CLIENT_H_
#define _UPLINK_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>
#include <uplink_session/capability.h>
#include <packet_stream_tx/client.h>
#include <packet_stream_rx/client.h>

namespace Uplink { class Session_client; }


class Uplink::Session_client : public Genode::Rpc_client<Session>
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


		/******************************
		 ** Uplink session interface **
		 ******************************/

		Tx *tx_channel() override { return &_tx; }
		Rx *rx_channel() override { return &_rx; }
		Tx::Source *tx() override { return _tx.source(); }
		Rx::Sink   *rx() override { return _rx.sink(); }
};

#endif /* _UPLINK_SESSION__CLIENT_H_ */
