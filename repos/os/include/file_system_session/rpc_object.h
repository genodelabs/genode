/*
 * \brief  Server-side file-system session interface
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__SERVER_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__SERVER_H_

#include <file_system_session/file_system_session.h>
#include <packet_stream_tx/rpc_object.h>
#include <base/rpc_server.h>

namespace File_system { class Session_rpc_object; }

class File_system::Session_rpc_object : public Genode::Rpc_object<Session, Session_rpc_object>
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
		Session_rpc_object(Genode::Dataspace_capability  tx_ds,
		                   Genode::Region_map           &rm,
		                   Genode::Rpc_entrypoint       &ep)
		: _tx(tx_ds, rm, ep) { }

		/**
		 * Return capability to packet-stream channel
		 *
		 * This method is called by the client via an RPC call at session
		 * construction time.
		 */
		Genode::Capability<Tx> _tx_cap() { return _tx.cap(); }

		Tx::Sink *tx_sink() { return _tx.sink(); }

		/**
		 * Default stub implementation
		 */
		Watch_handle watch(Path const &) override {
			throw Unavailable(); }

};

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__SERVER_H_ */
