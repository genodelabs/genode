/**
 * \brief  Server RPC object with packet stream
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__RPC_OBJECT_H_
#define _INCLUDE__USB_SESSION__RPC_OBJECT_H_

#include <base/rpc_server.h>
#include <packet_stream_tx/rpc_object.h>
#include <usb_session/usb_session.h>

namespace Usb { class Session_rpc_object; }


class Usb::Session_rpc_object : public Genode::Rpc_object<Session, Session_rpc_object>
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
		                   Genode::Rpc_entrypoint &ep,
		                   Genode::Region_map &rm)
		: _tx(tx_ds, rm, ep) { }

		/**
		 * Return capability to packet-stream channel
		 *
		 * This method is called by the client via an RPC call at session
		 * construction time.
		 */
		Genode::Capability<Tx> _tx_cap() { return _tx.cap(); }

		Tx::Sink *sink() { return _tx.sink(); }
};

#endif /* _INCLUDE__USB_SESSION__RPC_OBJECT_H_ */
