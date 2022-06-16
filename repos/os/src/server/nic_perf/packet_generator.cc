/*
 * \brief  Packet generator
 * \author Johannes Schlatow
 * \date   2022-06-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <packet_generator.h>
#include <interface.h>

void Nic_perf::Packet_generator::_handle_timeout(Genode::Duration)
{
	/* re-issue ARP request */
	if (_state == WAIT_ARP_REPLY)
		_state = NEED_ARP_REQUEST;

	_interface.handle_packet_stream();
}

void Nic_perf::Packet_generator::handle_arp_reply(Arp_packet const & arp)
{
	if (arp.src_ip() != _dst_ip)
		return;

	if (_state != WAIT_ARP_REPLY)
		return;

	_timeout.discard();

	_dst_mac = arp.src_mac();
	_state = READY;
}


void Nic_perf::Packet_generator::_generate_arp_request(void               * pkt_base,
                                                       Size_guard         & size_guard,
                                                       Mac_address  const & from_mac,
                                                       Ipv4_address const & from_ip)
{
	if (from_ip == Ipv4_address()) {
		error("Ip address not set");
		throw Ip_address_not_set();
	}

	Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
	eth.dst(Mac_address(0xff));
	eth.src(from_mac);
	eth.type(Ethernet_frame::Type::ARP);

	Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
	arp.hardware_address_type(Arp_packet::ETHERNET);
	arp.protocol_address_type(Arp_packet::IPV4);
	arp.hardware_address_size(sizeof(Mac_address));
	arp.protocol_address_size(sizeof(Ipv4_address));
	arp.opcode(Arp_packet::REQUEST);
	arp.src_mac(from_mac);
	arp.src_ip(from_ip);
	arp.dst_mac(Mac_address(0xff));
	arp.dst_ip(_dst_ip);
}


void Nic_perf::Packet_generator::_generate_test_packet(void               * pkt_base,
                                                       Size_guard         & size_guard,
                                                       Mac_address  const & from_mac,
                                                       Ipv4_address const & from_ip)
{
	if (from_ip == Ipv4_address()) {
		error("Ip address not set");
		throw Ip_address_not_set();
	}

	if (_dst_port == Port(0)) {
		error("Udp port not set");
		throw Udp_port_not_set();
	}

	Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
	eth.dst(_dst_mac);
	eth.src(from_mac);
	eth.type(Ethernet_frame::Type::IPV4);

	size_t const ip_off = size_guard.head_size();
	Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
	ip.header_length(sizeof(Ipv4_packet) / 4);
	ip.version(4);
	ip.time_to_live(64);
	ip.protocol(Ipv4_packet::Protocol::UDP);
	ip.src(from_ip);
	ip.dst(_dst_ip);

	size_t udp_off = size_guard.head_size();
	Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
	udp.src_port(Port(0));
	udp.dst_port(_dst_port);

	/* inflate packet up to _mtu */
	size_guard.consume_head(size_guard.unconsumed());

	/* fill in length fields and checksums */
	udp.length(size_guard.head_size() - udp_off);
	udp.update_checksum(ip.src(), ip.dst());
	ip.total_length(size_guard.head_size() - ip_off);
	ip.update_checksum();
}


void Nic_perf::Packet_generator::generate(void               * pkt_base,
                                          Size_guard         & size_guard,
                                          Mac_address  const & from_mac,
                                          Ipv4_address const & from_ip)
{
	switch (_state) {
		case READY:
			_generate_test_packet(pkt_base, size_guard, from_mac, from_ip);
			break;
		case NEED_ARP_REQUEST:
			_generate_arp_request(pkt_base, size_guard, from_mac, from_ip);
			_state = WAIT_ARP_REPLY;
			_timeout.schedule(Microseconds { 1000 * 1000 });
			break;
		case MUTED:
		case WAIT_ARP_REPLY:
			throw Not_ready();
			break;
	}
}
