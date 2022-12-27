/*
 * \brief  Network back-end towards public network (encrypted UDP tunnel)
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
#include <nic_connection.h>

/* os includes */
#include <net/udp.h>

using namespace Genode;
using namespace Net;
using namespace Wireguard;


Nic_connection::Handle_pkt_result
Nic_connection::_drop_pkt(char const *packet_type,
                          char const *reason)
{
	if (_verbose_pkt_drop) {
		log("Drop ", packet_type, " - ", reason);
	}
	return Handle_pkt_result::DROP_PACKET;
}


void Nic_connection::_connection_tx_flush_acks()
{
	while (_connection.tx()->ack_avail()) {
		_connection.tx()->release_packet(
			_connection.tx()->get_acked_packet());
	}
}


Nic_connection::Send_pkt_result
Nic_connection::_finish_send_eth_ipv4_with_eth_dst_set_via_arp(Packet_descriptor  pkt,
                                                               Mac_address const &eth_dst)
{
	void       *pkt_base   { _connection.tx()->packet_content(pkt) };
	Size_guard  size_guard { pkt.size() };

	Ethernet_frame &eth { Ethernet_frame::cast_from(pkt_base, size_guard) };

	eth.dst(eth_dst);
	_connection_tx_flush_acks();
	_connection.tx()->submit_packet(pkt);
	return Send_pkt_result::SUCCEEDED;
}


Nic_connection::Send_pkt_result
Nic_connection::_send_arp_reply(Ethernet_frame &request_eth,
                                Arp_packet     &request_arp)
{
	return send(ARP_PACKET_SIZE, [&] (void *reply_base, Size_guard &reply_guard) {

		Ethernet_frame &reply_eth {
			Ethernet_frame::construct_at(reply_base, reply_guard) };

		reply_eth.dst(request_eth.src());
		reply_eth.src(_mac_address);
		reply_eth.type(Ethernet_frame::Type::ARP);

		Arp_packet &reply_arp {
			reply_eth.construct_at_data<Arp_packet>(reply_guard) };

		reply_arp.hardware_address_type(Arp_packet::ETHERNET);
		reply_arp.protocol_address_type(Arp_packet::IPV4);
		reply_arp.hardware_address_size(sizeof(Mac_address));
		reply_arp.protocol_address_size(sizeof(Ipv4_address));
		reply_arp.opcode(Arp_packet::REPLY);
		reply_arp.src_mac(_mac_address);
		reply_arp.src_ip(request_arp.dst_ip());
		reply_arp.dst_mac(request_arp.src_mac());
		reply_arp.dst_ip(request_arp.src_ip());
	});
}


Nic_connection::Handle_pkt_result
Nic_connection::_handle_arp_request(Ethernet_frame &eth,
                                    Arp_packet     &arp)
{
	if (ip_config().interface().address != arp.dst_ip()) {
		return _drop_pkt("ARP request", "Doesn't target my IP address");
	}
	if (_send_arp_reply(eth, arp) != Send_pkt_result::SUCCEEDED) {
		return _drop_pkt("ARP request", "Sending reply failed");
	}
	return Handle_pkt_result::ACK_PACKET;
}


Nic_connection::Handle_pkt_result
Nic_connection::_handle_arp(Ethernet_frame &eth,
                            Size_guard     &size_guard)
{
	Arp_packet &arp { eth.data<Arp_packet>(size_guard) };
	if (!arp.ethernet_ipv4()) {
		return _drop_pkt("ARP packet", "Targets unknown protocol");
	}
	switch (arp.opcode()) {
	case Arp_packet::REQUEST: return _handle_arp_request(eth, arp);
	case Arp_packet::REPLY:   return _handle_arp_reply(arp);
	default:                  return _drop_pkt("ARP packet", "Unexpected opcode");
	}
}


void Nic_connection::_broadcast_arp_request(Ipv4_address const &src_ip,
                                            Ipv4_address const &dst_ip)
{
	send(ARP_PACKET_SIZE, [&] (void *pkt_base, Size_guard &size_guard) {

		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(Mac_address(0xff));
		eth.src(_mac_address);
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REQUEST);
		arp.src_mac(_mac_address);
		arp.src_ip(src_ip);
		arp.dst_mac(Mac_address(0xff));
		arp.dst_ip(dst_ip);
	});
}


Nic_connection::Handle_pkt_result
Nic_connection::_handle_arp_reply(Arp_packet &arp)
{
	_arp_cache.find_by_ip(arp.src_ip()).with_result(
		[&] (Const_pointer<Arp_cache_entry>) { },
		[&] (Arp_cache_error) {

			/* by now, no matching ARP cache entry exists, so create one */
			Ipv4_address const ip = arp.src_ip();
			_arp_cache.new_entry(ip, arp.src_mac());

			/* finish sending packets that waited for the entry */
			for (Arp_waiter_list_element *waiter_le = _arp_waiters.first();
				 waiter_le; ) {

				Arp_waiter &waiter = *waiter_le->object();
				waiter_le = waiter_le->next();
				if (ip != waiter.ip()) {
					continue;
				}
				_finish_send_eth_ipv4_with_eth_dst_set_via_arp(
					waiter.packet(), arp.src_mac());

				destroy(_alloc, &waiter);
			}
		}
	);
	return Handle_pkt_result::ACK_PACKET;
}


Nic_connection::Nic_connection(Env                       &env,
                               Allocator                 &alloc,
                               Signal_context_capability  pkt_stream_sigh,
                               Xml_node const            &config_node,
                               Timer::Connection         &timer,
                               Nic_connection_notifier   &notifier)
:
	_alloc              { alloc },
	_notifier           { notifier },
	_dhcp_client        { timer, *this },
	_ip_config          { config_node },
	_connection         { env, &_packet_alloc, BUF_SIZE, BUF_SIZE,
	                      "nic_session" },
	_link_state_handler { env.ep(), *this, &Nic_connection::_handle_link_state }
{
	_connection.rx_channel()->sigh_ready_to_ack(pkt_stream_sigh);
	_connection.rx_channel()->sigh_packet_avail(pkt_stream_sigh);
	_connection.tx_channel()->sigh_ack_avail(pkt_stream_sigh);
	_connection.tx_channel()->sigh_ready_to_submit(pkt_stream_sigh);
	_connection.link_state_sigh(_link_state_handler);

	if (ip_config().valid()) {
		_notifier.notify_about_ip_config_update();
	} else {
		_dhcp_client.discover();
	}
}


void Nic_connection::_handle_link_state()
{
	discard_ip_config();
	_dhcp_client.discover();
}


void Nic_connection::for_each_rx_packet(Handle_packet_func handle_packet)
{
	typename Nic::Connection::Rx::Sink & rx_sink = *_connection.rx();

	for (;;) {

		if (!rx_sink.packet_avail() || !rx_sink.ack_slots_free())
			break;

		Packet_descriptor const packet = rx_sink.peek_packet();

		bool const packet_valid = rx_sink.packet_valid(packet)
		                       && (packet.offset() >= 0);

		if (packet_valid) {

			void *eth_base { rx_sink.packet_content(packet) };
			Size_guard size_guard { packet.size() };
			Ethernet_frame &eth { Ethernet_frame::cast_from(eth_base, size_guard) };

			if (!ip_config().valid()) {

				_dhcp_client.handle_eth(eth, size_guard);

			} else {

				switch (eth.type()) {
				case Ethernet_frame::Type::ARP:

					_handle_arp(eth, size_guard);
					break;

				case Ethernet_frame::Type::IPV4:

					handle_packet(eth_base, packet.size());
					_notify_peers = true;
					break;

				default:

					_drop_pkt("packet", "Unknown type in Ethernet header");
					break;
				}
			}
		}
		(void)rx_sink.try_get_packet();
		rx_sink.try_ack_packet(packet);
	}
}


void Nic_connection::notify_peer()
{
	if (_notify_peers) {
		_notify_peers = false;
		_connection.rx()->wakeup();
		_connection.tx()->wakeup();
	}
}


void Nic_connection::discard_ip_config()
{
	_ip_config.construct();
	_notifier.notify_about_ip_config_update();
}


void Nic_connection::ip_config_from_dhcp_ack(Net::Dhcp_packet &dhcp_ack)
{
	_ip_config.construct(dhcp_ack);
	_notifier.notify_about_ip_config_update();
}


void
Nic_connection::send_wg_prot(uint8_t const *wg_prot_base,
                             size_t         wg_prot_size,
                             uint16_t       udp_src_port_big_endian,
                             uint16_t       udp_dst_port_big_endian,
                             uint32_t,
                             uint32_t       ipv4_dst_addr_big_endian,
                             uint8_t        ipv4_dscp_ecn,
                             uint8_t        ipv4_ttl)
{
	size_t const pkt_size {
		sizeof(Ethernet_frame) + sizeof(Ipv4_packet) + sizeof(Udp_packet) +
		wg_prot_size };

	Ipv4_address const dst_ip {
		Ipv4_address::from_uint32_big_endian(ipv4_dst_addr_big_endian) };

	_send_eth_ipv4_with_eth_dst_set_via_arp(
		pkt_size, dst_ip, [&] (Ethernet_frame &eth, Size_guard &size_guard)
	{
		/* create ETH header */
		eth.src(_mac_address);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the reply */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(ipv4_ttl);
		ip.diff_service_ecn(ipv4_dscp_ecn);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src_big_endian(ip_config().interface().address.to_uint32_big_endian());
		ip.dst_big_endian(ipv4_dst_addr_big_endian);

		/* create UDP header of the reply */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port_big_endian(udp_src_port_big_endian);
		udp.dst_port_big_endian(udp_dst_port_big_endian);

		/* add Wireguard protocol data */
		udp.memcpy_to_data((void *)wg_prot_base, wg_prot_size, size_guard);

		/* fill in header values that need the packet to be complete already */
		udp.length((uint16_t)(size_guard.head_size() - udp_off));
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}
