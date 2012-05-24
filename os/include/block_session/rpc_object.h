/*
 * \brief  Server-side block session interface
 * \author Stefan Kalkowski
 * \date   2010-07-06
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK_SESSION__SERVER_H_
#define _INCLUDE__BLOCK_SESSION__SERVER_H_

#include <block_session/block_session.h>
#include <packet_stream_tx/rpc_object.h>
#include <base/rpc_server.h>

namespace Block {

	class Session_rpc_object : public Genode::Rpc_object<Session, Session_rpc_object>
	{
		protected:

			Packet_stream_tx::Rpc_object<Tx> _tx;

		public:

			/**
			 * Constructor
			 *
			 * \param tx_ds  dataspace used as communication buffer
			 *               for the tx packet stream
			 * \param ep     entry point used for packet-stream channel
			 */
			Session_rpc_object(Genode::Dataspace_capability tx_ds,
			                   Genode::Rpc_entrypoint &ep)
			: _tx(tx_ds, ep) { }

			/**
			 * Return capability to packet-stream channel
			 *
			 * This function is called by the client via an RPC call at session
			 * construction time.
			 */
			Genode::Capability<Tx> _tx_cap() { return _tx.cap(); }

			Tx::Sink *tx_sink() { return _tx.sink(); }
	};
}

#endif /* _INCLUDE__BLOCK_SESSION__SERVER_H_ */
