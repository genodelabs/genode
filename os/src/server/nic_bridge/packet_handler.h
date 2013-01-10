/*
 * \brief  Thread implementations handling network packets.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PACKET_HANDLER_H_
#define _PACKET_HANDLER_H_

/* Genode */
#include <base/semaphore.h>
#include <base/thread.h>
#include <nic_session/connection.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net {

	/**
	 * Generic thread-implementation used as base for
	 * global receiver thread and client's transmit-threads.
	 */
	class Packet_handler : public Genode::Thread<8192>
	{
		private:

			Genode::Semaphore _startup_sem;       /* thread startup sync */

		protected:

			Nic::Connection            *_session; /* session to nic driver */
			Ethernet_frame::Mac_address _mac;     /* real nic's mac */

			/**
			 * Broadcasts ethernet frame to all clients,
			 * as long as its really a broadcast packtet.
			 *
			 * \param eth   ethernet frame to send.
			 * \param size  ethernet frame's size.
			 */
			void inline broadcast_to_clients(Ethernet_frame *eth,
			                                 Genode::size_t size);

			/**
			 * Send ethernet frame to NIC driver.
			 *
			 * \param eth   ethernet frame to send.
			 * \param size  ethernet frame's size.
			 */
			void send_to_nic(Ethernet_frame *eth, Genode::size_t size);

			/**
			 * Acknowledge the last processed packet.
			 */
			virtual void acknowledge_last_one()            = 0;

			/**
			 * Block for the next packet to process.
			 */
			virtual void next_packet(void** src,
			                         Genode::size_t *size) = 0;

			/*
			 * Handle an ARP packet
			 *
			 * \param eth   ethernet frame containing the ARP packet.
			 * \param size  ethernet frame's size.
			 */
			virtual bool handle_arp(Ethernet_frame *eth,
			                        Genode::size_t size)   = 0;

			/*
			 * Handle an IP packet
			 *
			 * \param eth   ethernet frame containing the IP packet.
			 * \param size  ethernet frame's size.
			 */
			virtual bool handle_ip(Ethernet_frame *eth,
			                       Genode::size_t size)    = 0;

			/*
			 * Finalize handling of ethernet frame.
			 *
			 * \param eth   ethernet frame to handle.
			 * \param size  ethernet frame's size.
			 */
			virtual void finalize_packet(Ethernet_frame *eth,
			                             Genode::size_t size) {}

		public:

			Packet_handler(Nic::Connection *session)
			: _session(session), _mac(session->mac_address().addr) {}

			/*
			 * Thread's entry code.
			 */
			void entry();

			/*
			 * Block until thread is ready to execute.
			 */
			void wait_for_startup() { _startup_sem.down(); }
	};


	/**
	 * Receiver thread handling network packets from the NIC driver
	 */
	class Rx_handler : public Packet_handler
	{
		private:

			Packet_descriptor _rx_packet; /* actual processed packet */

			/**
			 * Acknowledge the last processed packet.
			 */
			void acknowledge_last_one();

			/**
			 * Block for the next packet to process.
			 *
			 * \param src  buffer the network packet gets written to.
			 * \param size size of packet gets written to.
			 */
			void next_packet(void** src, Genode::size_t *size);

			/*
			 * Handle an ARP packet.
			 * If it's an ARP request for an IP of one of our client's,
			 * reply directly with the NIC's MAC address.
			 *
			 * \param eth   ethernet frame containing the ARP packet.
			 * \param size  ethernet frame's size.
			 */
			bool handle_arp(Ethernet_frame *eth, Genode::size_t size);

			/*
			 * Handle an IP packet.
			 * IP packets have to be inspected for DHCP replies.
			 * If an reply contains a new IP address for one of our
			 * client's, the client's database needs to be updated.
			 *
			 * \param eth   ethernet frame containing the IP packet.
			 * \param size  ethernet frame's size.
			 */
			bool handle_ip(Ethernet_frame *eth, Genode::size_t size);

		public:

			/**
			 * Constructor
			 *
			 * \param session  session to NIC driver.
			 */
			Rx_handler(Nic::Connection *session) : Packet_handler(session) {}
	};
}

#endif /* _PACKET_HANDLER_H_ */
