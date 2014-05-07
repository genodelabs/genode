/*
 * \brief  Signal driven NIC packet handler
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
	class Packet_handler;
}

/**
 * Generic packet handler used as base for NIC and client packet handlers.
 */
class Net::Packet_handler
{
	private:

		Packet_descriptor _packet;

		/**
		 * submit queue not empty anymore
		 */
		void _ready_to_submit(unsigned);

		/**
		 * acknoledgement queue not full anymore
		 *
		 * TODO: by now, we assume ACK and SUBMIT queue to be equally
		 *       dimensioned. That's why we ignore this signal by now.
		 */
		void _ack_avail(unsigned) { }

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ready_to_ack(unsigned);

		/**
		 * submit queue not full anymore
		 *
		 * TODO: by now, we just drop packets that cannot be transferred
		 *       to the other side, that's why we ignore this signal.
		 */
		void _packet_avail(unsigned) { }

	protected:

		Genode::Signal_dispatcher<Packet_handler> _sink_ack;
		Genode::Signal_dispatcher<Packet_handler> _sink_submit;
		Genode::Signal_dispatcher<Packet_handler> _source_ack;
		Genode::Signal_dispatcher<Packet_handler> _source_submit;

	public:

		Packet_handler();

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;


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
		 * Send ethernet frame
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void send(Ethernet_frame *eth, Genode::size_t size);

		/**
		 * Handle an ethernet packet
		 *
		 * \param src   ethernet frame's address
		 * \param size  ethernet frame's size.
		 */
		void handle_ethernet(void* src, Genode::size_t size);

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
		                             Genode::size_t size) = 0;
};

#endif /* _PACKET_HANDLER_H_ */
