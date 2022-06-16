/*
 * \brief  Base class for Nic/Uplink session components
 * \author Johannes Schlatow
 * \date   2022-06-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <interface.h>

/* Genode includes */
#include <net/mac_address.h>
#include <net/ipv4.h>
#include <net/udp.h>


void Nic_perf::Interface::_handle_eth(void * pkt_base, size_t size)
{
	try {

		Size_guard size_guard(size);
		Ethernet_frame &eth = Ethernet_frame::cast_from(pkt_base, size_guard);

		switch (eth.type()) {
			case Ethernet_frame::Type::ARP:
				_handle_arp(eth, size_guard);
				break;
			case Ethernet_frame::Type::IPV4:
				_handle_ip(eth, size_guard);
				break;
			default:
				;
		}
	} catch (Size_guard::Exceeded) {
		warning("Size guard exceeded");
	} catch (Net::Drop_packet_inform e) {
		error(e.msg);
	}

	_stats.rx_packet(size);
}


void Nic_perf::Interface::_handle_arp(Ethernet_frame & eth, Size_guard & size_guard)
{
	Arp_packet &arp = eth.data<Arp_packet>(size_guard);
	if (!arp.ethernet_ipv4())
		return;

	Ipv4_address old_src_ip { };

	switch (arp.opcode()) {
	case Arp_packet::REPLY:
		_generator.handle_arp_reply(arp);

		break;

	case Arp_packet::REQUEST:
		/* check whether the request targets us */
		if (arp.dst_ip() != _ip)
			return;

		old_src_ip = arp.src_ip();
		arp.opcode(Arp_packet::REPLY);
		arp.dst_mac(arp.src_mac());
		arp.src_mac(_mac);
		arp.src_ip(arp.dst_ip());
		arp.dst_ip(old_src_ip);
		eth.dst(arp.dst_mac());
		eth.src(_mac);

		send(size_guard.total_size(), [&] (void * pkt_base, Size_guard & size_guard) {
			memcpy(pkt_base, (void*)&eth, size_guard.total_size());
		});
		break;

	default:
		;
	}
}


void Nic_perf::Interface::_handle_ip(Ethernet_frame & eth, Size_guard & size_guard)
{
	Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
	if (ip.protocol() == Ipv4_packet::Protocol::UDP) {

		Udp_packet &udp = ip.data<Udp_packet>(size_guard);
		if (Dhcp_packet::is_dhcp(&udp)) {
			Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
			switch (dhcp.op()) {
			case Dhcp_packet::REQUEST:
				_handle_dhcp_request(eth, dhcp);
				break;
			case Dhcp_packet::REPLY:
				if (_dhcp_client.constructed()) {
					_dhcp_client->handle_dhcp(dhcp, eth, size_guard);
				}
				break;
			}
		}
	}
}


void Nic_perf::Interface::_handle_dhcp_request(Ethernet_frame & eth, Dhcp_packet & dhcp)
{
	Dhcp_packet::Message_type const msg_type =
		dhcp.option<Dhcp_packet::Message_type_option>().value();

	switch (msg_type) {
	case Dhcp_packet::Message_type::DISCOVER:
		_send_dhcp_reply(eth, dhcp, Dhcp_packet::Message_type::OFFER);
		break;
	case Dhcp_packet::Message_type::REQUEST:
		_send_dhcp_reply(eth, dhcp, Dhcp_packet::Message_type::ACK);
		break;
	default:
		;
	}
}


void Nic_perf::Interface::_send_dhcp_reply(Ethernet_frame      const & eth_req,
                                           Dhcp_packet         const & dhcp_req,
                                           Dhcp_packet::Message_type   msg_type)
{
	if (_ip == Ipv4_address())
		return;

	if (_dhcp_client_ip == Ipv4_address())
		return;

	enum { PKT_SIZE = 512 };
	send(PKT_SIZE, [&] (void *pkt_base,  Size_guard &size_guard) {

		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		if (msg_type == Dhcp_packet::Message_type::OFFER) {
			eth.dst(Ethernet_frame::broadcast()); }
		else {
			eth.dst(eth_req.src()); }
		eth.src(_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the reply */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(64);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src(_ip);
		ip.dst(_dhcp_client_ip);

		/* create UDP header of the reply */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port(Port(Dhcp_packet::BOOTPS));
		udp.dst_port(Port(Dhcp_packet::BOOTPC));

		/* create mandatory DHCP fields of the reply  */
		Dhcp_packet &dhcp = udp.construct_at_data<Dhcp_packet>(size_guard);
		dhcp.op(Dhcp_packet::REPLY);
		dhcp.htype(Dhcp_packet::Htype::ETH);
		dhcp.hlen(sizeof(Mac_address));
		dhcp.xid(dhcp_req.xid());
		if (msg_type == Dhcp_packet::Message_type::INFORM) {
			dhcp.ciaddr(_dhcp_client_ip); }
		else {
			dhcp.yiaddr(_dhcp_client_ip); }
		dhcp.siaddr(_ip);
		dhcp.client_mac(dhcp_req.client_mac());
		dhcp.default_magic_cookie();

		/* append DHCP option fields to the reply */
		Dhcp_packet::Options_aggregator<Size_guard> dhcp_opts(dhcp, size_guard);
		dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
		dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(_ip);
		dhcp_opts.append_option<Dhcp_packet::Ip_lease_time>(86400);
		dhcp_opts.append_option<Dhcp_packet::Subnet_mask>(_subnet_mask());
		dhcp_opts.append_option<Dhcp_packet::Router_ipv4>(_ip);

		dhcp_opts.append_dns_server([&] (Dhcp_options::Dns_server_data &data) {
			data.append_address(_ip);
		});
		dhcp_opts.append_option<Dhcp_packet::Broadcast_addr>(Ipv4_packet::broadcast());
		dhcp_opts.append_option<Dhcp_packet::Options_end>();

		/* fill in header values that need the packet to be complete already */
		udp.length(size_guard.head_size() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}


void Nic_perf::Interface::handle_packet_stream()
{
	/* handle acks from client */
	while (_source.ack_avail())
		_source.release_packet(_source.try_get_acked_packet());

	/* loop while we can make Rx progress */
	for (;;) {
		if (!_sink.ready_to_ack())
			break;

		if (!_sink.packet_avail())
			break;

		Packet_descriptor const packet_from_client = _sink.try_get_packet();

		if (_sink.packet_valid(packet_from_client)) {
			_handle_eth(_sink.packet_content(packet_from_client), packet_from_client.size());
			if (!_sink.try_ack_packet(packet_from_client))
				break;
		}
	}

	/* skip sending if disabled or IP address is not set */
	if (!_generator.enabled() || _ip == Ipv4_address()) {
		_sink.wakeup();
		_source.wakeup();
		return;
	}

	/* loop while we can make Tx progress */
	for (;;) {
		/*
		 * The client fails to pick up the packets from the rx channel. So we
		 * won't try to submit new packets.
		 */
		if (!_source.ready_to_submit())
			break;

		bool okay =
			send(_generator.size(), [&] (void * pkt_base, Size_guard & size_guard) {
				_generator.generate(pkt_base, size_guard, _mac, _ip);
			});

		if (!okay)
			break;
	}

	_sink.wakeup();
	_source.wakeup();
}
