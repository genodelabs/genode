/*
 * \brief  Interface definition for packet-stream reception channel
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_
#define _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_

#include <os/packet_stream.h>
#include <base/rpc.h>

namespace Packet_stream_rx { template <typename> struct Channel; }


template <typename PACKET_STREAM_POLICY>
struct Packet_stream_rx::Channel : Genode::Interface
{
	typedef Genode::Packet_stream_source<PACKET_STREAM_POLICY> Source;
	typedef Genode::Packet_stream_sink<PACKET_STREAM_POLICY>   Sink;

	/**
	 * Request reception interface
	 *
	 * See documentation of 'Packet_stream_tx::Cannel::source'.
	 */
	virtual Sink *sink() { return 0; }

	/**
	 * Register signal handler for receiving 'ready_to_ack' signals
	 */
	virtual void sigh_ready_to_ack(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Register signal handler for receiving 'packet_avail' signals
	 */
	virtual void sigh_packet_avail(Genode::Signal_context_capability sigh) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_packet_avail, void, sigh_packet_avail, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_ready_to_ack, void, sigh_ready_to_ack, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_ready_to_submit, Genode::Signal_context_capability, sigh_ready_to_submit);
	GENODE_RPC(Rpc_ack_avail, Genode::Signal_context_capability, sigh_ack_avail);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_packet_avail, Rpc_ready_to_ack,
	                     Rpc_ready_to_submit, Rpc_ack_avail);
};

#endif /* _INCLUDE__PACKET_STREAM_RX__PACKET_STREAM_RX_H_ */
