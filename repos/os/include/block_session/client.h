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
	private:

		Packet_stream_tx::Client<Tx> _tx;

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
			_tx(call<Rpc_tx_cap>(), rm, tx_buffer_alloc)
		{ }


		/*****************************
		 ** Block session interface **
		 *****************************/

		void info(sector_t *blk_count, Genode::size_t *blk_size,
		          Operations *ops) override
		{
			call<Rpc_info>(blk_count, blk_size, ops);
		}

		Tx *tx_channel() { return &_tx; }
		Tx::Source *tx() { return _tx.source(); }
		void sync() override { call<Rpc_sync>(); }

		/*
		 * Wrapper for alloc_packet, allocates 2KB aligned packets
		 */
		Packet_descriptor dma_alloc_packet(Genode::size_t size)
		{
			return tx()->alloc_packet(size, 11);
		}
};

#endif /* _INCLUDE__BLOCK_SESSION__CLIENT_H_ */
