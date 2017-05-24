/*
 * \brief  NIC session interface
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC_SESSION__NIC_SESSION_H_
#define _INCLUDE__NIC_SESSION__NIC_SESSION_H_

#include <dataspace/capability.h>
#include <base/signal.h>
#include <base/rpc.h>
#include <session/session.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <packet_stream_rx/packet_stream_rx.h>
#include <net/mac_address.h>

namespace Nic {

	using Mac_address = Net::Mac_address;

	struct Session;

	using Genode::Packet_stream_sink;
	using Genode::Packet_stream_source;

	typedef Genode::Packet_descriptor Packet_descriptor;
}


/*
 * NIC session interface
 *
 * A NIC session corresponds to a network adaptor, which can be used to
 * transmit and receive network packets. Payload is communicated over the
 * packet-stream interface set up between 'Session_client' and
 * 'Session_server'.
 *
 * Even though the methods 'tx', 'tx_channel', 'rx', and 'rx_channel' are
 * specific for the client side of the NIC session interface, they are part of
 * the abstract 'Session' class to enable the client-side use of the NIC
 * interface via a pointer to the abstract 'Session' class. This way, we can
 * transparently co-locate the packet-stream server with the client in same
 * program.
 */
struct Nic::Session : Genode::Session
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
	static const char *service_name() { return "Nic"; }

	/*
	 * A NIC session consumes a dataspace capability for the server-side
	 * session object, a session capability, two packet-stream dataspaces for
	 * rx and tx, and four signal context capabilities for the data-flow
	 * signals.
	 */
	enum { CAP_QUOTA = 8 };

	virtual ~Session() { }

	/**
	 * Request MAC address of network adapter
	 */
	virtual Mac_address mac_address() = 0;

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

	/**
	 * Request current link state of network adapter (true means link detected)
	 */
	virtual bool link_state() = 0;

	/**
	 * Register signal handler for link state changes
	 */
	virtual void link_state_sigh(Genode::Signal_context_capability sigh) = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_mac_address, Mac_address, mac_address);
	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
	GENODE_RPC(Rpc_rx_cap, Genode::Capability<Rx>, _rx_cap);
	GENODE_RPC(Rpc_link_state, bool, link_state);
	GENODE_RPC(Rpc_link_state_sigh, void, link_state_sigh,
	           Genode::Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_mac_address, Rpc_link_state,
	                     Rpc_link_state_sigh, Rpc_tx_cap, Rpc_rx_cap);
};

#endif /* _INCLUDE__NIC_SESSION__NIC_SESSION_H_ */
