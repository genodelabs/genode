/*
 * \brief  NIC session interface
 * \author Norman Feske
 * \date   2009-11-13
 *
 * A NIC session corresponds to a network adaptor, which can be used to
 * transmit and receive network packets. Payload is communicated over the
 * packet-stream interface set up between 'Session_client' and
 * 'Session_server'.
 *
 * Even though the functions 'tx', 'tx_channel', 'rx', and 'rx_channel' are
 * specific for the client side of the NIC session interface, they are part of
 * the abstract 'Session' class to enable the client-side use of the NIC
 * interface via a pointer to the abstract 'Session' class. This way, we can
 * transparently co-locate the packet-stream server with the client in same
 * program.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC_SESSION__NIC_SESSION_H_
#define _INCLUDE__NIC_SESSION__NIC_SESSION_H_

#include <dataspace/capability.h>
#include <base/signal.h>
#include <base/rpc.h>
#include <session/session.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <packet_stream_rx/packet_stream_rx.h>

namespace Nic {

	struct Mac_address { char addr[6]; };

	struct Session : Genode::Session
	{
		enum { QUEUE_SIZE = 1024 };

		/*
		 * Types used by the client stub code and server implementation
		 *
		 * The acknowledgement queue has always the same size as the submit
		 * queue. We access the packet content as a char pointer.
		 */
		typedef Packet_stream_policy<Packet_descriptor, QUEUE_SIZE, QUEUE_SIZE,
		                             char> Policy;

		typedef Packet_stream_tx::Channel<Policy> Tx;
		typedef Packet_stream_rx::Channel<Policy> Rx;

		static const char *service_name() { return "Nic"; }

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


		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_mac_address, Mac_address, mac_address);
		GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
		GENODE_RPC(Rpc_rx_cap, Genode::Capability<Rx>, _rx_cap);

		GENODE_RPC_INTERFACE(Rpc_mac_address, Rpc_tx_cap, Rpc_rx_cap);
	};
}

#endif /* _INCLUDE__NIC_SESSION__NIC_SESSION_H_ */
