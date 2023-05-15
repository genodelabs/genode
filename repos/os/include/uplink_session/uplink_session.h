/*
 * \brief  Uplink session interface
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_SESSION__UPLINK_SESSION_H_
#define _UPLINK_SESSION__UPLINK_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <packet_stream_rx/packet_stream_rx.h>

namespace Uplink {

	struct Session;

	using Genode::Packet_stream_sink;
	using Genode::Packet_stream_source;

	typedef Genode::Packet_descriptor Packet_descriptor;
}


/*
 * Uplink session interface
 *
 * An Uplink session corresponds to a network adaptor, which can be used to
 * transmit and receive network packets. Payload is communicated over the
 * packet-stream interface set up between 'Session_client' and
 * 'Session_server'.
 *
 * Even though the methods 'tx', 'tx_channel', 'rx', and 'rx_channel' are
 * specific for the client side of the Uplink session interface, they are part of
 * the abstract 'Session' class to enable the client-side use of the Uplink
 * interface via a pointer to the abstract 'Session' class. This way, we can
 * transparently co-locate the packet-stream server with the client in same
 * program.
 */
struct Uplink::Session : Genode::Session
{
	enum { QUEUE_SIZE = 1024 };

	/*
	 * Types used by the client stub code and server implementation
	 *
	 * The acknowledgement queue has always the same size as the submit
	 * queue. We access the packet content as a char pointer.
	 */
	typedef Genode::Packet_stream_policy<Genode::Packet_descriptor,
	                                     QUEUE_SIZE, QUEUE_SIZE, char> Policy;

	typedef Packet_stream_tx::Channel<Policy> Tx;
	typedef Packet_stream_rx::Channel<Policy> Rx;

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Uplink"; }

	/*
	 * An Uplink session consumes a dataspace capability for the server-side
	 * session object, a session capability, two packet-stream dataspaces for
	 * rx and tx, and four signal context capabilities for the data-flow
	 * signals.
	 */
	static constexpr unsigned CAP_QUOTA = 8;

	virtual ~Session() { }

	/**
	 * Request packet-transmission channel
	 */
	virtual Tx *tx_channel() { return 0; }

	/**
	 * Request packet-reception channel
	 */
	virtual Rx *rx_channel() { return 0; }

	/**
	 * Request client-side packet-stream interface of tx channel
	 */
	virtual Tx::Source *tx() { return 0; }

	/**
	 * Request client-side packet-stream interface of rx channel
	 */
	virtual Rx::Sink *rx() { return 0; }


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
	GENODE_RPC(Rpc_rx_cap, Genode::Capability<Rx>, _rx_cap);

	GENODE_RPC_INTERFACE(Rpc_tx_cap, Rpc_rx_cap);
};

#endif /* _UPLINK_SESSION__UPLINK_SESSION_H_ */
