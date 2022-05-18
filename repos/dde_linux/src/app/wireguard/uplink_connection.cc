/*
 * \brief  Network back-end towards private network (unencrypted user packets)
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* dde_linux wireguard includes */
#include <uplink_connection.h>

/* os includes */
#include <net/ipv4.h>

using namespace Genode;
using namespace Net;
using namespace Wireguard;


void Uplink_connection::_connection_tx_flush_acks()
{
	while (_connection.tx()->ack_avail()) {
		_connection.tx()->release_packet(
			_connection.tx()->get_acked_packet());
	}
}


void Uplink_connection::send_ip(void const *ip_base,
                                size_t      ip_size)
{
	Size_guard ip_guard { ip_size };
	Ipv4_packet const &ip { Ipv4_packet::cast_from(ip_base, ip_guard) };
	if (ip.version() != 4) {
		log("Drop packet - IP versions other than 4 not supported");
		return;
	}
	size_t const pkt_size { sizeof(Ethernet_frame) + ip_size };

	_send(pkt_size, [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* create ETH header */
		Ethernet_frame &eth {
			Ethernet_frame::construct_at(pkt_base, size_guard) };

		eth.src(_mac_address);
		eth.dst(Ethernet_frame::broadcast());
		eth.type(Ethernet_frame::Type::IPV4);

		/* add IP packet as payload */
		eth.memcpy_to_data((void *)ip_base, ip_size, size_guard);
	});
}


void Uplink_connection::notify_peer()
{
	if (_notify_peers) {
		_notify_peers = false;
		_connection.rx()->wakeup();
		_connection.tx()->wakeup();
	}
}


void Uplink_connection::for_each_rx_packet(Handle_packet_func handle_packet)
{
	typename Uplink::Connection::Rx::Sink & rx_sink = *_connection.rx();

	for (;;) {

		if (!rx_sink.packet_avail() || !rx_sink.ack_slots_free())
			break;

		Packet_descriptor const packet = rx_sink.peek_packet();

		bool const packet_valid = rx_sink.packet_valid(packet)
		                       && (packet.offset() >= 0);

		if (packet_valid) {

			void *eth_base { rx_sink.packet_content(packet) };

			handle_packet(eth_base, packet.size());
			_notify_peers = true;
		}
		(void)rx_sink.try_get_packet();
		rx_sink.try_ack_packet(packet);
	}
}



Uplink_connection::Uplink_connection(Env                       &env,
                                     Allocator                 &alloc,
                                     Signal_context_capability  sigh)
:
	_packet_alloc { &alloc },
	_connection   { env, &_packet_alloc, BUF_SIZE, BUF_SIZE, _mac_address,
	                "uplink_session" }
{
	_connection.rx_channel()->sigh_ready_to_ack(sigh);
	_connection.rx_channel()->sigh_packet_avail(sigh);
	_connection.tx_channel()->sigh_ack_avail(sigh);
	_connection.tx_channel()->sigh_ready_to_submit(sigh);
}
