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

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

using namespace Net;
using namespace Genode;

namespace Net {

	using Packet_descriptor    = ::Nic::Packet_descriptor;
	using Packet_stream_sink   = ::Nic::Packet_stream_sink< ::Nic::Session::Policy>;
	using Packet_stream_source = ::Nic::Packet_stream_source< ::Nic::Session::Policy>;
}

Microseconds read_sec_attr(Xml_node      const  node,
                           char          const *name,
                           unsigned long const  default_sec)
{
	unsigned long sec = node.attribute_value(name, 0UL);
	if (!sec) {
		sec = default_sec;
	}
	return Microseconds(sec * 1000 * 1000);
}


class Main
{
	private:

		using Signal_handler   = Genode::Signal_handler<Main>;
		using Periodic_timeout = Timer::Periodic_timeout<Main>;

		enum { IPV4_TIME_TO_LIVE  = 64 };
		enum { ICMP_ID            = 1166 };
		enum { ICMP_DATA_SIZE     = 56 };
		enum { DEFAULT_COUNT      = 5 };
		enum { DEFAULT_PERIOD_SEC = 5 };
		enum { PKT_SIZE           = Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
		enum { BUF_SIZE           = Nic::Session::QUEUE_SIZE * PKT_SIZE };

		Env                    &_env;
		Attached_rom_dataspace  _config_rom    { _env, "config" };
		Xml_node                _config        { _config_rom.xml() };
		Timer::Connection       _timer         { _env };
		Microseconds            _send_time     { 0 };
		Periodic_timeout        _period        { _timer, *this, &Main::_send_ping,
		                                         read_sec_attr(_config, "period_sec", DEFAULT_PERIOD_SEC) };
		Heap                    _heap          { &_env.ram(), &_env.rm() };
		Nic::Packet_allocator   _pkt_alloc     { &_heap };
		Nic::Connection         _nic           { _env, &_pkt_alloc, BUF_SIZE, BUF_SIZE };
		Signal_handler          _sink_ack      { _env.ep(), *this, &Main::_ack_avail };
		Signal_handler          _sink_submit   { _env.ep(), *this, &Main::_ready_to_submit };
		Signal_handler          _source_ack    { _env.ep(), *this, &Main::_ready_to_ack };
		Signal_handler          _source_submit { _env.ep(), *this, &Main::_packet_avail };
		bool             const  _verbose       { _config.attribute_value("verbose", false) };
		Ipv4_address     const  _src_ip        { _config.attribute_value("src_ip",  Ipv4_address()) };
		Ipv4_address     const  _dst_ip        { _config.attribute_value("dst_ip",  Ipv4_address()) };
		Mac_address      const  _src_mac       { _nic.mac_address() };
		Mac_address             _dst_mac       { };
		uint16_t                _ip_id         { 1 };
		uint16_t                _icmp_seq      { 1 };
		unsigned long           _count         { _config.attribute_value("count", (unsigned long)DEFAULT_COUNT) };

		void _handle_eth(void *const  eth_base,
		                 Size_guard  &size_guard);

		void _handle_ip(Ethernet_frame &eth,
		                Size_guard     &size_guard);

		void _handle_icmp(Ipv4_packet  &ip,
		                  Size_guard   &size_guard);

		void _handle_icmp_echo_reply(Ipv4_packet &ip,
		                             Icmp_packet &icmp,
		                             Size_guard  &size_guard);

		void _handle_icmp_dst_unreachbl(Ipv4_packet &ip,
		                                Icmp_packet &icmp,
		                                Size_guard  &size_guard);

		void _handle_arp(Ethernet_frame &eth,
		                 Size_guard     &size_guard);

		void _broadcast_arp_request();

		void _send_arp_reply(Ethernet_frame &req_eth,
		                     Arp_packet     &req_arp);

		template <typename FUNC>
		void _send(size_t  pkt_size,
		           FUNC && write_to_pkt)
		{
			try {
				Packet_descriptor  pkt      = _source().alloc_packet(pkt_size);
				void              *pkt_base = _source().packet_content(pkt);
				Size_guard size_guard(pkt_size);
				write_to_pkt(pkt_base, size_guard);
				_source().submit_packet(pkt);
				if (_verbose) {
					try {
						Size_guard size_guard(pkt_size);
						log("snd ", Ethernet_frame::cast_from(pkt_base, size_guard));
					}
					catch (Size_guard::Exceeded) { log("snd ?"); }
				}
			}
			catch (Net::Packet_stream_source::Packet_alloc_failed) {
				warning("failed to allocate packet"); }
		}

		void _send_ping(Duration not_used = Duration(Microseconds(0)));

		Net::Packet_stream_sink   &_sink()   { return *_nic.rx(); }
		Net::Packet_stream_source &_source() { return *_nic.tx(); }


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit();
		void _ack_avail() { }
		void _ready_to_ack();
		void _packet_avail() { }

	public:

		struct Invalid_arguments : Exception { };

		Main(Env &env);
};


Main::Main(Env &env) : _env(env)
{
	/* exit unsuccessful if parameters are invalid */
	if (_src_ip == Ipv4_address() ||
	    _dst_ip == Ipv4_address() ||
	    _count  == 0)
	{
		throw Invalid_arguments();
	}
	/* install packet stream signals */
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
}


void Main::_handle_eth(void *const  eth_base,
                       Size_guard  &size_guard)
{
	/* print receipt message */
	Ethernet_frame &eth = Ethernet_frame::cast_from(eth_base, size_guard);
	if (_verbose) {
		log("rcv ", eth); }

	/* drop packet if ETH does not target us */
	if (eth.dst() != _src_mac &&
	    eth.dst() != Ethernet_frame::broadcast())
	{
		if (_verbose) {
			log("bad ETH destination"); }
		return;
	}
	/* select ETH sub-protocol */
	switch (eth.type()) {
	case Ethernet_frame::Type::ARP:  _handle_arp(eth, size_guard); break;
	case Ethernet_frame::Type::IPV4: _handle_ip(eth, size_guard);  break;
	default: ; }
}


void Main::_handle_ip(Ethernet_frame &eth,
                      Size_guard     &size_guard)
{
	/* drop packet if IP does not target us */
	Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
	if (ip.dst() != _src_ip &&
	    ip.dst() != Ipv4_packet::broadcast())
	{
		if (_verbose) {
			log("bad IP destination"); }
		return;
	}
	/* drop packet if IP checksum is invalid */
	if (ip.checksum_error()) {
		if (_verbose) {
			log("bad IP checksum"); }
		return;
	}
	/* select IP sub-protocol */
	switch (ip.protocol()) {
	case Ipv4_packet::Protocol::ICMP: _handle_icmp(ip, size_guard);
	default: ; }
}


void Main::_handle_icmp_echo_reply(Ipv4_packet &ip,
                                   Icmp_packet &icmp,
                                   Size_guard  &size_guard)
{
	/* check IP source */
	if (ip.src() != _dst_ip) {
		if (_verbose) {
			log("bad IP source"); }
		return;
	}
	/* check ICMP code */
	if (icmp.code() != Icmp_packet::Code::ECHO_REPLY) {
		if (_verbose) {
			log("bad ICMP type/code"); }
		return;
	}
	/* check ICMP identifier */
	uint16_t const icmp_id  = icmp.query_id();
	if (icmp_id != ICMP_ID) {
		if (_verbose) {
			log("bad ICMP identifier"); }
		return;
	}
	/* check ICMP sequence number */
	uint16_t const icmp_seq  = icmp.query_seq();
	if (icmp_seq != _icmp_seq) {
		if (_verbose) {
			log("bad ICMP sequence number"); }
		return;
	}
	/* check ICMP data size */
	if (size_guard.unconsumed() != ICMP_DATA_SIZE) {
		if (_verbose) {
			log("bad ICMP data size"); }
		return;
	}
	/* check ICMP data */
	struct Data { char chr[0]; };
	Data &data = icmp.data<Data>(size_guard);
	char chr = 'a';
	for (addr_t chr_id = 0; chr_id < ICMP_DATA_SIZE; chr_id++) {
		if (data.chr[chr_id] != chr) {
			if (_verbose) {
				log("bad ICMP data"); }
			return;
		}
		chr = chr < 'z' ? chr + 1 : 'a';
	}
	/* calculate time since the request was sent */
	unsigned long time_us = _timer.curr_time().trunc_to_plain_us().value - _send_time.value;
	unsigned long const time_ms = time_us / 1000UL;
	time_us = time_us - time_ms * 1000UL;

	/* print success message */
	log(ICMP_DATA_SIZE + sizeof(Icmp_packet), " bytes from ", ip.src(),
	    ": icmp_seq=", icmp_seq, " ttl=", (unsigned long)IPV4_TIME_TO_LIVE,
	    " time=", time_ms, ".", time_us ," ms");

	/* raise ICMP sequence number and check exit condition */
	_icmp_seq++;
	_count--;
	if (!_count) {
		_env.parent().exit(0); }
}


void Main::_handle_icmp_dst_unreachbl(Ipv4_packet &ip,
                                      Icmp_packet &icmp,
                                      Size_guard  &size_guard)
{
	/* drop packet if embedded IP checksum is invalid */
	Ipv4_packet &embed_ip = icmp.data<Ipv4_packet>(size_guard);
	if (embed_ip.checksum_error()) {
		if (_verbose) {
			log("bad IP checksum in payload of ICMP error"); }
		return;
	}
	/* drop packet if the ICMP error is not about ICMP */
	if (embed_ip.protocol() != Ipv4_packet::Protocol::ICMP) {
		if (_verbose) {
			log("bad IP protocol in payload of ICMP error"); }
		return;
	}
	/* drop packet if embedded ICMP identifier is invalid */
	Icmp_packet &embed_icmp = embed_ip.data<Icmp_packet>(size_guard);
	if (embed_icmp.query_id() != ICMP_ID) {
		if (_verbose) {
			log("bad ICMP identifier in payload of ICMP error"); }
		return;
	}
	/* drop packet if embedded ICMP sequence number is invalid */
	uint16_t const embed_icmp_seq = embed_icmp.query_seq();
	if (embed_icmp_seq != _icmp_seq) {
		if (_verbose) {
			log("bad ICMP sequence number in payload of ICMP error"); }
		return;
	}
	log("From ", ip.src(), " icmp_seq=", embed_icmp_seq, " Destination Unreachable");
}


void Main::_handle_icmp(Ipv4_packet &ip,
                        Size_guard  &size_guard)
{
	/* drop packet if ICMP checksum is invalid */
	Icmp_packet &icmp = ip.data<Icmp_packet>(size_guard);
	if (icmp.checksum_error(size_guard.unconsumed())) {
		if (_verbose) {
			log("bad ICMP checksum"); }
		return;
	}
	/* select ICMP type */
	switch (icmp.type()) {
	case Icmp_packet::Type::ECHO_REPLY:      _handle_icmp_echo_reply(ip, icmp, size_guard); break;
	case Icmp_packet::Type::DST_UNREACHABLE: _handle_icmp_dst_unreachbl(ip, icmp, size_guard); break;
	default:
		if (_verbose) {
			log("bad ICMP type"); }
		return;
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
		if (_dst_mac != Mac_address() || arp.src_ip() != _dst_ip) {
			return; }

		/* set destination MAC address and retry to ping */
		_dst_mac = arp.src_mac();
		_send_ping();
		return;

	case Arp_packet::REQUEST:

		/* check whether the ARP request targets us */
		if (arp.dst_ip() != _src_ip) {
			return; }

		_send_arp_reply(eth, arp);

	default: ; }
}


void Main::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		Size_guard size_guard(pkt.size());
		_handle_eth(_sink().packet_content(pkt), size_guard);

		if (!_sink().ready_to_ack()) {
			error("ack state FULL");
			return;
		}
		_sink().acknowledge_packet(pkt);
	}
}


void Main::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


void Main::_send_arp_reply(Ethernet_frame &req_eth,
                           Arp_packet     &req_arp)
{
	_send(sizeof(Ethernet_frame) + sizeof(Arp_packet),
	      [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(req_eth.src());
		eth.src(_src_mac);
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REPLY);
		arp.src_mac(_src_mac);
		arp.src_ip(_src_ip);
		arp.dst_mac(req_eth.src());
		arp.dst_ip(req_arp.src_ip());
	});
}


void Main::_broadcast_arp_request()
{
	_send(sizeof(Ethernet_frame) + sizeof(Arp_packet),
	      [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(Mac_address(0xff));
		eth.src(_src_mac);
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REQUEST);
		arp.src_mac(_src_mac);
		arp.src_ip(_src_ip);
		arp.dst_mac(Mac_address(0xff));
		arp.dst_ip(_dst_ip);
	});
}


void Main::_send_ping(Duration)
{
	if (_dst_mac == Mac_address()) {
		_broadcast_arp_request();
		return;
	}
	_send(sizeof(Ethernet_frame) + sizeof(Ipv4_packet) +
	      sizeof(Icmp_packet) + ICMP_DATA_SIZE,
	      [&] (void *pkt_base, Size_guard &size_guard)
	{
		/* create ETH header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(_dst_mac);
		eth.src(_src_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.protocol(Ipv4_packet::Protocol::ICMP);
		ip.src(_src_ip);
		ip.dst(_dst_ip);

		/* create ICMP header */
		Icmp_packet &icmp = ip.construct_at_data<Icmp_packet>(size_guard);
		icmp.type(Icmp_packet::Type::ECHO_REQUEST);
		icmp.code(Icmp_packet::Code::ECHO_REQUEST);
		icmp.query_id(ICMP_ID);
		icmp.query_seq(_icmp_seq);

		/* fill ICMP data with characters from 'a' to 'z' */
		struct Data { char chr[ICMP_DATA_SIZE]; };
		Data &data = icmp.data<Data>(size_guard);
		char chr = 'a';
		for (addr_t chr_id = 0; chr_id < ICMP_DATA_SIZE; chr_id++) {
			data.chr[chr_id] = chr;
			chr = chr < 'z' ? chr + 1 : 'a';
		}
		/* fill in header values that require the packet to be complete */
		icmp.update_checksum(ICMP_DATA_SIZE);
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
	_send_time = _timer.curr_time().trunc_to_plain_us();
}


void Component::construct(Env &env) { static Main main(env); }
