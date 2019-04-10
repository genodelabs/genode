/*
 * \brief  Client-side block session interface
 * \author Stefan Kalkowski
 * \date   2010-07-06
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CLIENT_H_
#define _INCLUDE__BLOCK_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <block_session/capability.h>
#include <packet_stream_tx/client.h>

namespace Block { class Session_client; }


class Block::Session_client : public Genode::Rpc_client<Session>
{
	protected:

		Packet_stream_tx::Client<Tx> _tx;

		Info const _info = info();

	public:

		/**
		 * Constructor
		 *
		 * \param session          session capability
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 */
		Session_client(Session_capability       session,
		               Genode::Range_allocator &tx_buffer_alloc,
		               Genode::Region_map      &rm)
		:
			Genode::Rpc_client<Session>(session),
			_tx(tx_cap(), rm, tx_buffer_alloc)
		{ }


		/*****************************
		 ** Block session interface **
		 *****************************/

		Info info() const override { return call<Rpc_info>(); }

		Tx *tx_channel() override { return &_tx; }

		Tx::Source *tx() override { return _tx.source(); }

		Genode::Capability<Tx> tx_cap() override { return call<Rpc_tx_cap>(); }

		/**
		 * Allocate packet respecting the server's alignment constraints
		 */
		Packet_descriptor alloc_packet(Genode::size_t size)
		{
			return tx()->alloc_packet(size, _info.align_log2);
		}
};

#endif /* _INCLUDE__BLOCK_SESSION__CLIENT_H_ */
