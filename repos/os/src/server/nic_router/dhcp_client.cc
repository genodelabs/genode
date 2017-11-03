/*
 * \brief  DHCP client state model
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dhcp_client.h>
#include <interface.h>
#include <domain.h>
#include <configuration.h>
#include <size_guard.h>

using namespace Genode;
using namespace Net;
using Message_type   = Dhcp_packet::Message_type;
using Packet_ignored = Interface::Packet_ignored;


Configuration &Dhcp_client::_config() { return _domain().config(); };


Domain &Dhcp_client::_domain() { return _interface.domain(); }


Dhcp_client::Dhcp_client(Genode::Allocator &alloc,
                         Timer::Connection &timer,
                         Interface         &interface)
:
	_alloc(alloc), _interface(interface),
	_timeout(timer, *this, &Dhcp_client::_handle_timeout)
{ }


void Dhcp_client::discover()
{
	_set_state(State::SELECT, _config().rtt());
	_send(Message_type::DISCOVER, Ipv4_address(), Ipv4_address());
}


void Dhcp_client::_rerequest(State next_state)
{
	_set_state(next_state, _rerequest_timeout(2));
	_send(Message_type::REQUEST, _domain().ip_config().interface.address,
	      Ipv4_address());
}


void Dhcp_client::_set_state(State state, Microseconds timeout)
{
	_state = state;
	_timeout.schedule(timeout);
}


Microseconds Dhcp_client::_rerequest_timeout(unsigned lease_time_div_log2)
{
	/* FIXME limit the time because of shortcomings in timeout framework */
	enum { MAX_TIMEOUT_SEC = 3600 };
	unsigned long timeout_sec = _lease_time_sec >> lease_time_div_log2;

	if (timeout_sec > MAX_TIMEOUT_SEC) {
		timeout_sec = MAX_TIMEOUT_SEC;
		warning("Had to prune the state timeout of DHCP client");
	}
	return Microseconds(timeout_sec * 1000UL * 1000UL);
}


void Dhcp_client::_handle_timeout(Duration)
{
	switch (_state) {
	case State::BOUND:  _rerequest(State::RENEW);  break;
	case State::RENEW:  _rerequest(State::REBIND); break;
	case State::REBIND: _domain().discard_ip_config();
	default:            discover();
	}
}


void Dhcp_client::handle_ip(Ethernet_frame &eth, size_t eth_size)
{
	if (eth.dst() != _interface.router_mac() &&
	    eth.dst() != Mac_address(0xff))
	{
		throw Packet_ignored("DHCP client expects Ethernet targeting the router");
	}
	Ipv4_packet &ip = *new (eth.data<void>())
		Ipv4_packet(eth_size - sizeof(Ethernet_frame));

	if (ip.protocol() != Ipv4_packet::Protocol::UDP) {
		throw Packet_ignored("DHCP client expects UDP packet");
	}
	Udp_packet &udp = *new (ip.data<void>())
		Udp_packet(eth_size - sizeof(Ipv4_packet));

	if (!Dhcp_packet::is_dhcp(&udp)) {
		throw Packet_ignored("DHCP client expects DHCP packet");
	}
	Dhcp_packet &dhcp = *new (udp.data<void>())
		Dhcp_packet(eth_size - sizeof(Ipv4_packet) - sizeof(Udp_packet));

	if (dhcp.op() != Dhcp_packet::REPLY) {
		throw Packet_ignored("DHCP client expects DHCP reply");
	}
	if (dhcp.client_mac() != _interface.router_mac()) {
		throw Packet_ignored("DHCP client expects DHCP targeting the router");
	}
	try { _handle_dhcp_reply(dhcp); }
	catch (Dhcp_packet::Option_not_found) {
		throw Packet_ignored("DHCP client misses DHCP option");
	}
}


void Dhcp_client::_handle_dhcp_reply(Dhcp_packet &dhcp)
{
	Message_type const msg_type =
		dhcp.option<Dhcp_packet::Message_type_option>().value();

	switch (_state) {
	case State::SELECT:

		if (msg_type != Message_type::OFFER) {
			throw Packet_ignored("DHCP client expects an offer");
		}
		_set_state(State::REQUEST, _config().rtt());
		_send(Message_type::REQUEST, dhcp.yiaddr(),
		      dhcp.option<Dhcp_packet::Server_ipv4>().value());
		break;

	case State::REQUEST:

		if (msg_type != Message_type::ACK) {
			throw Packet_ignored("DHCP client expects an acknowledgement");
		}
		_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
		_set_state(State::BOUND, _rerequest_timeout(1));
		_domain().ip_config(dhcp.yiaddr(),
		                    dhcp.option<Dhcp_packet::Subnet_mask>().value(),
		                    dhcp.option<Dhcp_packet::Router_ipv4>().value());
		break;

	case State::RENEW:
	case State::REBIND:

		if (msg_type != Message_type::ACK) {
			throw Packet_ignored("DHCP client expects an acknowledgement");
		}
		_set_state(State::BOUND, _rerequest_timeout(1));
		_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
		break;

	default: throw Packet_ignored("DHCP client doesn't expect a packet");
	}
}


void Dhcp_client::_send(Message_type msg_type,
                        Ipv4_address client_ip,
                        Ipv4_address server_ip)
{
	/* allocate buffer for the request */
	enum { BUF_SIZE = 1024 };
	using Size_guard = Size_guard_tpl<BUF_SIZE, Interface::Dhcp_msg_buffer_too_small>;
	void *buf;
	try { _alloc.alloc(BUF_SIZE, &buf); }
	catch (...) { throw Interface::Alloc_dhcp_msg_buffer_failed(); }
	Mac_address client_mac = _interface.router_mac();

	/* create ETH header of the request */
	Size_guard size;
	size.add(sizeof(Ethernet_frame));
	Ethernet_frame &eth = *reinterpret_cast<Ethernet_frame *>(buf);
	eth.dst(Mac_address(0xff));
	eth.src(client_mac);
	eth.type(Ethernet_frame::Type::IPV4);

	/* create IP header of the request */
	enum { IPV4_TIME_TO_LIVE = 64 };
	size_t const ip_off = size.curr();
	size.add(sizeof(Ipv4_packet));
	Ipv4_packet &ip = *eth.data<Ipv4_packet>();
	ip.header_length(sizeof(Ipv4_packet) / 4);
	ip.version(4);
	ip.diff_service(0);
	ip.identification(0);
	ip.flags(0);
	ip.fragment_offset(0);
	ip.time_to_live(IPV4_TIME_TO_LIVE);
	ip.protocol(Ipv4_packet::Protocol::UDP);
	ip.src(client_ip);
	ip.dst(Ipv4_address(0xff));

	/* create UDP header of the request */
	size_t const udp_off = size.curr();
	size.add(sizeof(Udp_packet));
	Udp_packet &udp = *ip.data<Udp_packet>();
	udp.src_port(Port(Dhcp_packet::BOOTPC));
	udp.dst_port(Port(Dhcp_packet::BOOTPS));

	/* create mandatory DHCP fields of the request  */
	size_t const dhcp_off = size.curr();
	size.add(sizeof(Dhcp_packet));
	Dhcp_packet &dhcp = *udp.data<Dhcp_packet>();
	dhcp.op(Dhcp_packet::REQUEST);
	dhcp.htype(Dhcp_packet::Htype::ETH);
	dhcp.hlen(sizeof(Mac_address));
	dhcp.hops(0);
	dhcp.xid(0x12345678);
	dhcp.secs(0);
	dhcp.flags(0);
	dhcp.ciaddr(client_ip);
	dhcp.yiaddr(Ipv4_address());
	dhcp.siaddr(Ipv4_address());
	dhcp.giaddr(Ipv4_address());
	dhcp.client_mac(client_mac);
	dhcp.zero_fill_sname();
	dhcp.zero_fill_file();
	dhcp.default_magic_cookie();

	/* append DHCP option fields to the request */
	Dhcp_packet::Options_aggregator<Size_guard>
		dhcp_opts(dhcp, size);
	dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);

	switch (msg_type) {
	case Message_type::DISCOVER:
		dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
		dhcp_opts.append_option<Dhcp_packet::Max_msg_size>(BUF_SIZE - dhcp_off);
		break;

	case Message_type::REQUEST:
		dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
		dhcp_opts.append_option<Dhcp_packet::Max_msg_size>(BUF_SIZE - dhcp_off);
		if (_state == State::REQUEST) {
			dhcp_opts.append_option<Dhcp_packet::Requested_addr>(client_ip);
			dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(server_ip);
		}
		break;

	default:
		throw Interface::Bad_send_dhcp_args();
	}
	dhcp_opts.append_option<Dhcp_packet::Options_end>();

	/* fill in header values that need the packet to be complete already */
	udp.length(size.curr() - udp_off);
	udp.update_checksum(ip.src(), ip.dst());
	ip.total_length(size.curr() - ip_off);
	ip.checksum(Ipv4_packet::calculate_checksum(ip));

	/* send request to sender of request and free request buffer */
	_interface.send(eth, size.curr());
	_alloc.free(buf, BUF_SIZE);
}
