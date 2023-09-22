/*
 * \brief  Periodically Request time from SNTP server and report it
 * \author Martin Stein
 * \date   2019-06-27
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
#include <tm.h>

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/sntp.h>
#include <net/dns.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <rtc_session/connection.h>
#include <os/reporter.h>

using namespace Net;
using namespace Genode;


Microseconds read_min_attr(Xml_node const  node,
                           char     const *name,
                           uint64_t const  default_sec)
{
	enum { MAX_MINUTES = (~(uint64_t)0) / 1000 / 1000 / 60 };
	uint64_t min = node.attribute_value(name, (uint64_t)0);
	if (!min) {
		min = default_sec;
	}
	if (min > MAX_MINUTES) {
		warning("minutes value exceeds maximum");
		min = MAX_MINUTES;
	}
	return Microseconds(min * 60 * 1000 * 1000);
}


class Main : public Nic_handler,
             public Dhcp_client_handler
{
	private:

		using Periodic_timeout = Timer::Periodic_timeout<Main>;

		enum { IPV4_TIME_TO_LIVE  = 64 };
		enum { DEFAULT_PERIOD_MIN = 60 };
		enum { SRC_PORT           = 50000 };

		Env                            &_env;
		Attached_rom_dataspace          _config_rom    { _env, "config" };
		Xml_node                        _config        { _config_rom.xml() };
		Timer::Connection               _timer         { _env };
		Microseconds                    _period_us     { read_min_attr(_config, "period_min", (uint64_t)DEFAULT_PERIOD_MIN) };
		Constructible<Periodic_timeout> _period        { };
		Heap                            _heap          { &_env.ram(), &_env.rm() };
		bool                     const  _verbose       { _config.attribute_value("verbose", false) };
		Net::Nic                        _nic           { _env, _heap, *this, _verbose };
		Ipv4_address                    _dst_ip        { _config.attribute_value("dst_addr", Ipv4_address()) };
		Domain_name              const  _dst_ns        { _config.attribute_value("dst_addr", Domain_name())  };
		unsigned short                  _dns_req_id    { 0 };
		Mac_address                     _dst_mac       { };
		Constructible<Dhcp_client>      _dhcp_client   { };
		Reconstructible<Ipv4_config>    _ip_config     { _config.attribute_value("interface", Ipv4_address_prefix()),
		                                                 _config.attribute_value("gateway",   Ipv4_address()),
		                                                 _config.attribute_value("dns-server",   Ipv4_address()) };
		Reporter reporter                              { _env, "set_rtc" };

		Sntp_timestamp _rtc_ts_to_sntp_ts(Rtc::Timestamp const rtc_ts)
		{
			struct tm tm { (int)rtc_ts.second,    (int)rtc_ts.minute,
			               (int)rtc_ts.hour,      (int)rtc_ts.day,
			               (int)rtc_ts.month - 1, (int)rtc_ts.year - 1900,
			               0, 0, 0, 0, nullptr };

			return Sntp_timestamp::from_unix_timestamp(tm_to_secs(&tm));
		}

		Rtc::Timestamp _sntp_ts_to_rtc_ts(Sntp_timestamp const sntp_ts)
		{
			struct tm tm { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr };
			if (secs_to_tm(sntp_ts.to_unix_timestamp(), &tm)) {
				warning("failed to convert timestamp");
				return Rtc::Timestamp();
			}
			return {
				0, (unsigned)tm.tm_sec,     (unsigned)tm.tm_min,
				   (unsigned)tm.tm_hour,    (unsigned)tm.tm_mday,
				   (unsigned)tm.tm_mon + 1, (unsigned)tm.tm_year + 1900 };
		}

		void _handle_ip(Ethernet_frame &eth,
		                Size_guard     &size_guard);

		void _handle_udp(Ipv4_packet &ip,
		                 Size_guard  &size_guard);

		void _handle_arp(Ethernet_frame &eth,
		                 Size_guard     &size_guard);

		void _handle_dns(Udp_packet &udp,
		                 Size_guard &size_guard);

		void _send_dns_request();

		void _broadcast_arp_request(Ipv4_address const &ip);

		void _send_arp_reply(Ethernet_frame &req_eth,
		                     Arp_packet     &req_arp);

		void _send_sntp_request(Duration not_used = Duration(Microseconds(0)));

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
	_period.construct(_timer, *this, &Main::_send_sntp_request, _period_us);
}


Main::Main(Env &env) : _env(env)
{
	/* deprecated dst_ip configuration option */
	if (_config.has_attribute("dst_ip")) {
		warning("\"dst_ip\" configuration attribute is deprecated, please use \"dst_addr\"");
		_dst_ip = _config.attribute_value("dst_ip", Ipv4_address());
		if (_dst_ip == Ipv4_address()) {
			throw Invalid_arguments(); } }

	/* exit unsuccessful if parameters are invalid */
	if (_dst_ns == Domain_name()) {
		if (_dst_ip == Ipv4_address()) {
			throw Invalid_arguments(); } }

	/* if there is a static IP config, start sending pings periodically */
	if (ip_config().valid) {
		_period.construct(_timer, *this, &Main::_send_sntp_request, _period_us); }

	/* else, start the DHCP client for requesting an IP config */
	else {
		_dhcp_client.construct(_heap, _timer, _nic, *this); }

	reporter.enabled(true);
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
		case Ethernet_frame::Type::ARP:  _handle_arp(eth, size_guard); break;
		case Ethernet_frame::Type::IPV4: _handle_ip(eth, size_guard);  break;
		default: ; }
	}
	catch (Drop_packet_inform exception) {
		if (_verbose) {
			log("drop packet: ", exception.msg); }
	}
}


void Main::_handle_ip(Ethernet_frame &eth,
                      Size_guard     &size_guard)
{
	/* drop packet if IP does not target us */
	Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
	if (ip.dst() != ip_config().interface.address &&
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
	case Ipv4_packet::Protocol::UDP:  _handle_udp(ip, size_guard); break;
	default: ; }
}


void Main::_handle_udp(Ipv4_packet &ip,
                       Size_guard  &size_guard)
{
	/* drop packet if UDP checksum is invalid */
	Udp_packet &udp = ip.data<Udp_packet>(size_guard);
	if (udp.checksum_error(ip.src(), ip.dst())) {
		if (_verbose) {
			log("bad UDP checksum"); }
		return;
	}

	if (udp.src_port().value == Dns_packet::UDP_PORT) {
		_handle_dns(udp, size_guard);
		return;
	}

	/* drop packet if UDP source port is invalid */
	if (udp.src_port().value != Sntp_packet::UDP_PORT) {
		if (_verbose) {
			log("bad UDP source port"); }
		return;
	}
	/* drop packet if UDP destination port is invalid */
	if (udp.dst_port().value != SRC_PORT) {
		if (_verbose) {
			log("bad UDP destination port"); }
		return;
	}

	Sntp_packet &sntp = udp.data<Sntp_packet>(size_guard);
	if (sntp.version_number() != Sntp_packet::VERSION_NUMBER) {
		if (_verbose) {
			log("bad SNTP version number"); }
		return;
	}
	if (sntp.mode() != Sntp_packet::MODE_SERVER) {
		if (_verbose) {
			log("bad SNTP mode"); }
		return;
	}

	Rtc::Timestamp rtc_ts { _sntp_ts_to_rtc_ts(sntp.transmit_timestamp()) };
	Reporter::Xml_generator xml(reporter, [&] () {
		xml.attribute("year",   rtc_ts.year);
		xml.attribute("month",  rtc_ts.month);
		xml.attribute("day",    rtc_ts.day);
		xml.attribute("hour",   rtc_ts.hour);
		xml.attribute("minute", rtc_ts.minute);
		xml.attribute("second", rtc_ts.second);
	});
	if (_dst_ns != Domain_name()) {
		_dst_ip = Ipv4_address();
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
		if (_dst_ip == Ipv4_address() && _dst_ns != Domain_name()) {
			_send_dns_request();
		} else {
			_send_sntp_request();
		}
		return;

	case Arp_packet::REQUEST:

		/* check whether the ARP request targets us */
		if (arp.dst_ip() != ip_config().interface.address) {
			return; }

		_send_arp_reply(eth, arp);

	default: ; }
}


void Main::_handle_dns(Udp_packet &udp,
                       Size_guard &size_guard)
{
	Dns_packet &dns = udp.data<Dns_packet>(size_guard);

	if (!dns.response()) {
		error("DNS message is not a response");
		return; }

	if (dns.id() != _dns_req_id) {
		if (_verbose) {
			log("unexpeted DNS request id in response"); }
		return; }

	try {

		dns.for_each_entry(size_guard, [&] (Dns_packet::Dns_entry const &entry) {
			if (_dst_ip == Ipv4_address()) {
				_dst_ip = entry.addr;
				if (_verbose) {
					Genode::log(entry.name, " resolved to ", entry.addr); } } });

	} catch (Size_guard::Exceeded const &) {
		error("Malformated DNS response");
		_dst_ip = Ipv4_address();
		return;
	}

	if (_dst_ip == Ipv4_address()) {
		if (_verbose) {
			Genode::log(_dst_ns, " could not be resolved."); }
	} else {
		_send_sntp_request();
	}
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


void Main::_send_dns_request()
{
	if (_ip_config->dns_server == Ipv4_address()) {
		throw Invalid_arguments(); } 

	if (_verbose) {
		Genode::log("Sending dns query for ", _dst_ns ," to ", ip_config().dns_server); }

	/* if we do not yet know the Ethernet destination, request it via ARP */
	if (_dst_mac == Mac_address()) {
		if (ip_config().interface.prefix_matches(ip_config().dns_server)) {
			_broadcast_arp_request(ip_config().dns_server); }
		else {
			_broadcast_arp_request(ip_config().gateway); }
		return;
	}

	++_dns_req_id;

	_nic.send(sizeof(Ethernet_frame) + sizeof(Ipv4_packet) +
	          sizeof(Udp_packet) + sizeof(Dns_packet) +
	          Dns_packet::sizeof_question(_dst_ns),
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
		ip.dst(ip_config().dns_server);

		/* adapt IP header to UDP */
		ip.protocol(Ipv4_packet::Protocol::UDP);

		/* create UDP header */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port(Port(SRC_PORT));
		udp.dst_port(Port(Dns_packet::UDP_PORT));

		/* create DNS header */
		Dns_packet &dns = udp.construct_at_data<Dns_packet>(size_guard);
		dns.id(_dns_req_id);
		dns.recursion_desired(true);
		dns.question(size_guard, _dst_ns);

		/* finish UDP header */
		udp.length(size_guard.head_size() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());

		/* finish IP header */
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});

}

void Main::_send_sntp_request(Duration)
{
	/* if we do not yet know the IP destination, resolve it */
	if (_dst_ip == Ipv4_address() && _dst_ns != Domain_name()) {
		_send_dns_request();
		return;
	}
	/* if we do not yet know the Ethernet destination, request it via ARP */
	if (_dst_mac == Mac_address()) {
		if (ip_config().interface.prefix_matches(_dst_ip)) {
			_broadcast_arp_request(_dst_ip); }
		else {
			_broadcast_arp_request(ip_config().gateway); }
		return;
	}
	_nic.send(sizeof(Ethernet_frame) + sizeof(Ipv4_packet) +
	          sizeof(Udp_packet) + sizeof(Sntp_packet),
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

		/* adapt IP header to UDP */
		ip.protocol(Ipv4_packet::Protocol::UDP);

		/* create UDP header */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port(Port(SRC_PORT));
		udp.dst_port(Port(Sntp_packet::UDP_PORT));

		/* create SNTP header */
		Sntp_packet &sntp = udp.construct_at_data<Sntp_packet>(size_guard);
		sntp.version_number(Sntp_packet::VERSION_NUMBER);
		sntp.mode(Sntp_packet::MODE_CLIENT);

		/* finish UDP header */
		udp.length(size_guard.head_size() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());

		/* finish IP header */
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}

void Component::construct(Env &env) { static Main main(env); }
