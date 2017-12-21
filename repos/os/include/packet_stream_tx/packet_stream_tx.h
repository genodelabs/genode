/*
 * \brief  Interface definition for packet-stream transmission channel
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_TX__PACKET_STREAM_TX_H_
#define _INCLUDE__PACKET_STREAM_TX__PACKET_STREAM_TX_H_

#include <os/packet_stream.h>
#include <base/rpc.h>

namespace Packet_stream_tx { template <typename> struct Channel; }


template <typename PACKET_STREAM_POLICY>
struct Packet_stream_tx::Channel : Genode::Interface
{
	typedef Genode::Packet_stream_source<PACKET_STREAM_POLICY> Source;
	typedef Genode::Packet_stream_sink<PACKET_STREAM_POLICY>   Sink;

	/**
	 * Request transmission interface
	 *
	 * This method enables the client-side use of the 'Channel' using
	 * the abstract 'Channel' interface only. This is useful in cases
	 * where both source and sink of the 'Channel' are co-located in
	 * one program. At the server side of the 'Channel', this method
	 * has no meaning.
	 */
	virtual Source *source() { return 0; }

	/**
	 * Register signal handler for receiving 'ready_to_submit' signals
	 */
	virtual void sigh_ready_to_submit(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Register signal handler for receiving 'ack_avail' signals
	 */
	virtual void sigh_ack_avail(Genode::Signal_context_capability sigh) = 0;


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_ack_avail, void, sigh_ack_avail, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_ready_to_submit, void, sigh_ready_to_submit, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_ready_to_ack, Genode::Signal_context_capability, sigh_ready_to_ack);
	GENODE_RPC(Rpc_packet_avail, Genode::Signal_context_capability, sigh_packet_avail);
	
	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_ack_avail, Rpc_ready_to_submit,
	                     Rpc_ready_to_ack, Rpc_packet_avail);
};

#endif /* _INCLUDE__PACKET_STREAM_TX__PACKET_STREAM_TX_H_ */
