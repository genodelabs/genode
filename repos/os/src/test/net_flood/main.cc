/*
 * \brief  Test the reachability of a host on an IP network
 * \author Martin Stein
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <nic.h>
#include <ipv4_config.h>
#include <dhcp_client.h>
#include <protocol.h>

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/tcp.h>
#include <net/icmp.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Net;
using namespace Genode;


class Main : public Nic_handler,
             public Dhcp_client_handler
{
	private:

		using Periodic_timeout = Timer::Periodic_timeout<Main>;

		enum { IPV4_TIME_TO_LIVE = 64 };
		enum { ICMP_DATA_SIZE    = 56 };
		enum { ICMP_SEQ          = 1 };
		enum { SRC_PORT          = 50000 };
		enum { FIRST_DST_PORT    = 49152 };
		enum { LAST_DST_PORT     = 65535 };

		Env                            &_env;
		Attached_rom_dataspace          _config_rom  { _env, "config" };
		Xml_node                        _config      { _config_rom.xml() };
		Timer::Connection               _timer       { _env };
		Microseconds                    _period_us   { 100 };
		Constructible<Periodic_timeout> _period      { };
		Heap                            _heap        { &_env.ram(), &_env.rm() };
		bool                     const  _verbose     { _config.attribute_value("verbose", false) };
		Net::Nic                        _nic         { _env, _heap, *this, _verbose };
		Ipv4_address             const  _dst_ip      { _config.attribute_value("dst_ip",  Ipv4_address()) };
		Mac_address                     _dst_mac     { };
		Constructible<Dhcp_client>      _dhcp_client { };
		Reconstructible<Ipv4_config>    _ip_config   { _config.attribute_value("interface", Ipv4_address_prefix()),
		                                               _config.attribute_value("gateway",   Ipv4_address()),
		                                               Ipv4_address() };
		Protocol                 const  _protocol    { _config.attribute_value("protocol", Protocol::ICMP) };
		Port                            _dst_port    { FIRST_DST_PORT };

		void _handle_arp(Ethernet_frame &eth,
		                 Size_guard     &size_guard);

		void _broadcast_arp_request(Ipv4_address const &ip);

		void _send_arp_reply(Ethernet_frame &req_eth,
		                     Arp_packet     &req_arp);

		void _send_ping(Duration not_used = Duration(Microseconds(0)));

	public:

		struct Invalid_arguments : Exception { };

		Main(Env &env);


		/*****************
		 ** Nic_handler **
		 *****************/

		void handle_eth(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;


		/*************************
		 ** Dhcp_client_handler **
		 *************************/

		void ip_config(Ipv4_config const &ip_config) override;

		Ipv4_config const &ip_config() const override { return *_ip_config; }
};


void Main::ip_config(Ipv4_config const &ip_config)
{
	if (_verbose) {
		log("IP config: ", ip_config); }

	_ip_config.construct(ip_config);
	_period.construct(_timer, *this, &Main::_send_ping, _period_us);
}


Main::Main(Env &env) : _env(env)
{
	/* exit unsuccessful if parameters are invalid */
	if (_dst_ip == Ipv4_address()) {
		throw Invalid_arguments(); }

	/* if there is a static IP config, start sending pings periodically */
	if (ip_config().valid) {
		_period.construct(_timer, *this, &Main::_send_ping, _period_us); }

	/* else, start the DHCP client for requesting an IP config */
	else {
		_dhcp_client.construct(_heap, _timer, _nic, *this); }
}


void Main::handle_eth(Ethernet_frame &eth,
                      Size_guard     &size_guard)
{
	try {
		/* print receipt message */
		if (_verbose) {
			log("rcv ", eth); }

		if (!ip_config().valid) {
			_dhcp_client->handle_eth(eth, size_guard); }

		/* drop packet if ETH does not target us */
		if (eth.dst() != _nic.mac() &&
			eth.dst() != Ethernet_frame::broadcast())
		{
			if (_verbose) {
				log("bad ETH destination"); }
			return;
		}
		/* select ETH sub-protocol */
		switch (eth.type()) {
		case Ethernet_frame::Type::ARP: _handle_arp(eth, size_guard); break;
		default: ; }
	}
	catch (Drop_packet_inform exception) {
		if (_verbose) {
			log("drop packet: ", exception.msg); }
	}
}


void Main::_handle_arp(Ethernet_frame &eth,
                       Size_guard     &size_guard)
{
	/* check ARP protocol- and hardware address type */
	Arp_packet &arp = eth.data<Arp_packet>(size_guard);
	if (!arp.ethernet_ipv4()) {
		error("ARP for unknown protocol"); }

	/* check ARP operation */
	switch (arp.opcode()) {
	case Arp_packet::REPLY:

		/* check whether we waited for this ARP reply */
		if (_dst_mac != Mac_address()) {
			return; }

		if (ip_config().interface.prefix_matches(_dst_ip)) {
			if (arp.src_ip() != _dst_ip) {
				return; }
		} else {
			if (arp.src_ip() != ip_config().gateway) {
				return; }
		}
		/* set destination MAC address and retry to ping */
		_dst_mac = arp.src_mac();
		return;

	case Arp_packet::REQUEST:

		/* check whether the ARP request targets us */
		if (arp.dst_ip() != ip_config().interface.address) {
			return; }

		_send_arp_reply(eth, arp);

	default: ; }
}


void Main::_send_arp_reply(Ethernet_frame &req_eth,
                           Arp_packet     &req_arp)
{
	_nic.send(sizeof(Ethernet_frame) + sizeof(Arp_packet),
	          [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(req_eth.src());
		eth.src(_nic.mac());
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REPLY);
		arp.src_mac(_nic.mac());
		arp.src_ip(ip_config().interface.address);
		arp.dst_mac(req_eth.src());
		arp.dst_ip(req_arp.src_ip());
	});
}


void Main::_broadcast_arp_request(Ipv4_address const &dst_ip)
{
	_nic.send(sizeof(Ethernet_frame) + sizeof(Arp_packet),
	          [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(Mac_address(0xff));
		eth.src(_nic.mac());
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REQUEST);
		arp.src_mac(_nic.mac());
		arp.src_ip(ip_config().interface.address);
		arp.dst_mac(Mac_address(0xff));
		arp.dst_ip(dst_ip);
	});
}


void Main::_send_ping(Duration)
{
	/* if we do not yet know the Ethernet destination, request it via ARP */
	if (_dst_mac == Mac_address()) {
		if (ip_config().interface.prefix_matches(_dst_ip)) {
			_broadcast_arp_request(_dst_ip); }
		else {
			_broadcast_arp_request(ip_config().gateway); }
		return;
	}
	_nic.send(sizeof(Ethernet_frame) + sizeof(Ipv4_packet) +
	          sizeof(Icmp_packet) + ICMP_DATA_SIZE,
	          [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* create ETH header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(_dst_mac);
		eth.src(_nic.mac());
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.src(ip_config().interface.address);
		ip.dst(_dst_ip);

		/* select IP-encapsulated protocol */
		switch (_protocol) {
		case Protocol::ICMP:
			{
				/* adapt IP header to ICMP */
				ip.protocol(Ipv4_packet::Protocol::ICMP);

				/* create ICMP header */
				Icmp_packet &icmp = ip.construct_at_data<Icmp_packet>(size_guard);
				icmp.type(Icmp_packet::Type::ECHO_REQUEST);
				icmp.code(Icmp_packet::Code::ECHO_REQUEST);
				icmp.query_id(_dst_port.value);
				icmp.query_seq(ICMP_SEQ);

				/* finish ICMP header */
				icmp.update_checksum(ICMP_DATA_SIZE);

				/* prepare next ICMP ping */
				if (_dst_port.value == LAST_DST_PORT) {
					_dst_port.value = FIRST_DST_PORT; }
				else {
					_dst_port.value++; }
				break;
			}
		case Protocol::UDP:
			{
				/* adapt IP header to UDP */
				ip.protocol(Ipv4_packet::Protocol::UDP);

				/* create UDP header */
				size_t const udp_off = size_guard.head_size();
				Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
				udp.src_port(Port(SRC_PORT));
				udp.dst_port(_dst_port);

				/* finish UDP header */
				udp.length(size_guard.head_size() - udp_off);
				udp.update_checksum(ip.src(), ip.dst());

				/* prepare next ping */
				if (_dst_port.value == LAST_DST_PORT) {
					_dst_port.value = FIRST_DST_PORT; }
				else {
					_dst_port.value++; }
				break;
			}
		case Protocol::TCP:
			{
				/* adapt IP header to TCP */
				ip.protocol(Ipv4_packet::Protocol::TCP);

				/* create TCP header */
				size_t const tcp_off = size_guard.head_size();
				Tcp_packet &tcp = ip.construct_at_data<Tcp_packet>(size_guard);
				tcp.src_port(Port(SRC_PORT));
				tcp.dst_port(_dst_port);

				/* finish TCP header */
				tcp.update_checksum(ip.src(), ip.dst(), size_guard.head_size() - tcp_off);

				/* prepare next ping */
				if (_dst_port.value == LAST_DST_PORT) {
					_dst_port.value = FIRST_DST_PORT; }
				else {
					_dst_port.value++; }
				break;
			}
		}
		/* finish IP header */
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}


void Component::construct(Env &env) { static Main main(env); }
