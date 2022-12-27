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

/* dde_linux wireguard includes */
#include <dhcp_client.h>
#include <nic_connection.h>

enum { PKT_SIZE = 1024 };

using namespace Genode;
using namespace Net;
using namespace Wireguard;

using Message_type = Dhcp_packet::Message_type;
using Dhcp_options = Dhcp_packet::Options_aggregator<Size_guard>;


/***************
 ** Utilities **
 ***************/

void append_param_req_list(Dhcp_options &dhcp_opts)
{
	dhcp_opts.append_param_req_list([&] (Dhcp_options::Parameter_request_list_data &data) {
		data.append_param_req<Dhcp_packet::Message_type_option>();
		data.append_param_req<Dhcp_packet::Server_ipv4>();
		data.append_param_req<Dhcp_packet::Ip_lease_time>();
		data.append_param_req<Dhcp_packet::Dns_server_ipv4>();
		data.append_param_req<Dhcp_packet::Domain_name>();
		data.append_param_req<Dhcp_packet::Subnet_mask>();
		data.append_param_req<Dhcp_packet::Router_ipv4>();
	});
}


/*****************
 ** Dhcp_client **
 *****************/

Dhcp_client::Dhcp_client(Timer::Connection &timer,
                         Nic_connection    &nic_connection)
:
	_nic_connection { nic_connection },
	_timeout        { timer, *this, &Dhcp_client::_handle_timeout }
{ }


void Dhcp_client::discover()
{
	_set_state(State::SELECT, _nic_connection.dhcp_discover_timeout());
	_send(Message_type::DISCOVER, Ipv4_address(), Ipv4_address(),
	      Ipv4_address());
}


void Dhcp_client::_rerequest(State next_state)
{
	_set_state(next_state, _rerequest_timeout(2));
	Ipv4_address const client_ip {
		_nic_connection.ip_config().interface().address };

	_send(Message_type::REQUEST, client_ip, Ipv4_address(), client_ip);
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
	uint64_t timeout_sec = _lease_time_sec >> lease_time_div_log2;

	if (timeout_sec > MAX_TIMEOUT_SEC) {
		timeout_sec = MAX_TIMEOUT_SEC;
		if (_nic_connection.verbose()) {
			log("DHCP client: prune re-request timeout");
		}
	}
	return Microseconds(timeout_sec * 1000 * 1000);
}


void Dhcp_client::_handle_timeout(Duration)
{
	switch (_state) {
	case State::BOUND:

		_rerequest(State::RENEW);
		break;

	case State::RENEW:

		_rerequest(State::REBIND);
		break;

	case State::REBIND:

		_nic_connection.discard_ip_config();
		discover();
		break;

	default:

		discover();
		break;
	}
}


void Dhcp_client::handle_eth(Ethernet_frame &eth,
                             Size_guard     &size_guard)
{
	if (eth.dst() != _nic_connection.mac_address() &&
	    eth.dst() != Mac_address(0xff))
	{
		log("DHCP client: drop packet - ETH.DST does not target me");
		return;
	}
	if (eth.type() != Ethernet_frame::Type::IPV4) {
		log("DHCP client: drop packet - ETH.TYPE is not IPV4");
		return;
	}
	Ipv4_packet &ip { eth.data<Ipv4_packet>(size_guard) };
	if (ip.protocol() != Ipv4_packet::Protocol::UDP) {
		log("DHCP client: drop packet - IPV4.PROTOCOL is not UDP");
		return;
	}
	Udp_packet &udp { ip.data<Udp_packet>(size_guard) };
	if (!Dhcp_packet::is_dhcp(&udp)) {
		log("DHCP client: drop packet - UDP does not carry DHCP");
		return;
	}
	Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
	if (dhcp.op() != Dhcp_packet::REPLY) {
		log("DHCP client: drop packet - DHCP.OP is not REPLY");
		return;
	}
	if (dhcp.client_mac() != _nic_connection.mac_address()) {
		log("DHCP client: drop packet - DHCP.CLIENT_MAC is not my MAC address");
		return;
	}
	try {
		Message_type const msg_type =
			dhcp.option<Dhcp_packet::Message_type_option>().value();

		switch (_state) {
		case State::SELECT:

			if (msg_type != Message_type::OFFER) {
				log("DHCP client: drop packet - DHCP.MSG_TYPE is not OFFER");
				return;
			}
			_set_state(State::REQUEST, _nic_connection.dhcp_request_timeout());
			_send(Message_type::REQUEST, Ipv4_address(),
			      dhcp.option<Dhcp_packet::Server_ipv4>().value(),
			      dhcp.yiaddr());

			break;

		case State::REQUEST:
			{
				if (msg_type != Message_type::ACK) {
					log("DHCP client: drop packet - DHCP.MSG_TYPE is not ACK (state REQUEST)");
					return;
				}
				_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
				_set_state(State::BOUND, _rerequest_timeout(1));
				_nic_connection.ip_config_from_dhcp_ack(dhcp);
				break;
			}
		case State::RENEW:
		case State::REBIND:

			if (msg_type != Message_type::ACK) {
				log("DHCP client: drop packet - DHCP.MSG_TYPE is not ACK (state RENEW/REBIND)");
				return;
			}
			_set_state(State::BOUND, _rerequest_timeout(1));
			_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
			break;

		default:

			log("DHCP client: drop packet - client doesn't expect reply");
			return;
		}
	}
	catch (Dhcp_packet::Option_not_found) {

		log("DHCP client: drop packet - DHCP.MSG_TYPE missing");
		return;
	}
}


void Dhcp_client::_send(Message_type msg_type,
                        Ipv4_address client_ip,
                        Ipv4_address server_ip,
                        Ipv4_address requested_ip)
{
	Mac_address client_mac = _nic_connection.mac_address();
	_nic_connection.send(PKT_SIZE, [&] (void *pkt_base, Size_guard &size_guard) {

		/* create ETH header of the request */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(Mac_address(0xff));
		eth.src(client_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the request */
		enum { IPV4_TIME_TO_LIVE = 64 };
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src(client_ip);
		ip.dst(Ipv4_address(0xff));

		/* create UDP header of the request */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port(Port(Dhcp_packet::BOOTPC));
		udp.dst_port(Port(Dhcp_packet::BOOTPS));

		/* create mandatory DHCP fields of the request  */
		size_t const dhcp_off = size_guard.head_size();
		Dhcp_packet &dhcp = udp.construct_at_data<Dhcp_packet>(size_guard);
		dhcp.op(Dhcp_packet::REQUEST);
		dhcp.htype(Dhcp_packet::Htype::ETH);
		dhcp.hlen(sizeof(Mac_address));
		dhcp.ciaddr(client_ip);
		dhcp.client_mac(client_mac);
		dhcp.default_magic_cookie();

		/* append DHCP option fields to the request */
		Dhcp_options dhcp_opts(dhcp, size_guard);
		dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
		switch (msg_type) {
		case Message_type::DISCOVER:
			append_param_req_list(dhcp_opts);
			dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
			dhcp_opts.append_option<Dhcp_packet::Max_msg_size>((uint16_t)(PKT_SIZE - dhcp_off));
			break;

		case Message_type::REQUEST:
			append_param_req_list(dhcp_opts);
			dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
			dhcp_opts.append_option<Dhcp_packet::Max_msg_size>((uint16_t)(PKT_SIZE - dhcp_off));
			if (_state == State::REQUEST) {
				dhcp_opts.append_option<Dhcp_packet::Requested_addr>(requested_ip);
				dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(server_ip);
			}
			break;

		default:
			class Bad_dhcp_message_type { };
			throw Bad_dhcp_message_type { };
		}
		dhcp_opts.append_option<Dhcp_packet::Options_end>();

		/* fill in header values that need the packet to be complete already */
		udp.length((uint16_t)(size_guard.head_size() - udp_off));
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}
