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

using namespace Genode;
using namespace Net;
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

Dhcp_client::Dhcp_client(Cached_timer      &timer,
                         Interface         &interface)
:
	_interface(interface),
	_timeout(timer, *this, &Dhcp_client::_handle_timeout)
{ }


void Dhcp_client::discover()
{
	enum { DISCOVER_PKT_SIZE = 309 };
	_set_state(State::SELECT, _interface.config().dhcp_discover_timeout());
	_send(Message_type::DISCOVER, Ipv4_address(), Ipv4_address(),
	      Ipv4_address(), DISCOVER_PKT_SIZE);
}


void Dhcp_client::_rerequest(State next_state, Domain &domain)
{
	enum { REREQUEST_PKT_SIZE = 309 };
	_set_state(next_state, _rerequest_timeout(2, domain));
	Ipv4_address client_ip = domain.ip_config().interface().address;
	_send(Message_type::REQUEST, client_ip, Ipv4_address(), client_ip,
	      REREQUEST_PKT_SIZE);
}


void Dhcp_client::_set_state(State state, Microseconds timeout)
{
	_state = state;
	_timeout.schedule(timeout);
}


Microseconds Dhcp_client::_rerequest_timeout(unsigned lease_time_div_log2, Domain &domain)
{
	/* FIXME limit the time because of shortcomings in timeout framework */
	enum { MAX_TIMEOUT_SEC = 3600 };
	uint64_t timeout_sec = _lease_time_sec >> lease_time_div_log2;

	if (timeout_sec > MAX_TIMEOUT_SEC) {
		timeout_sec = MAX_TIMEOUT_SEC;
		if (_interface.config().verbose())
			log("[", domain, "] prune re-request timeout of DHCP client");
	}
	return Microseconds(timeout_sec * 1000 * 1000);
}


void Dhcp_client::_handle_timeout(Duration)
{
	_interface.with_domain(
		[&] /* domain_fn */ (Domain &domain) {
			switch (_state) {
			case State::BOUND: _rerequest(State::RENEW, domain); break;
			case State::RENEW: _rerequest(State::REBIND, domain); break;
			case State::REBIND:

				domain.discard_ip_config();
				discover();
				break;

			default: discover(); break;
			}
		},
		[&] /* no_domain_fn */ {
			if (_interface.config().verbose())
				log("[?] no domain on DHCP timeout"); });
}


Packet_result Dhcp_client::handle_dhcp_reply(Dhcp_packet &dhcp, Domain &domain)
{
	Packet_result result { };
	auto no_msg_type_fn = [&] { result = packet_drop("DHCP request misses option \"Message Type\""); };
	auto msg_type_fn = [&] (Dhcp_packet::Message_type_option const &msg_type) {

		if (_interface.config().verbose_domain_state()) {
			if (msg_type.value() == Message_type::OFFER) {
				Ipv4_address dns_server, subnet_mask, router_ip;
				dhcp.with_option<Dhcp_packet::Dns_server_ipv4>([&] (auto opt) { dns_server = opt.value(); });
				dhcp.with_option<Dhcp_packet::Subnet_mask>([&] (auto opt) { subnet_mask = opt.value(); });
				dhcp.with_option<Dhcp_packet::Router_ipv4>([&] (auto opt) { router_ip = opt.value(); });

				log("[", domain, "] dhcp offer from ",
				    dhcp.siaddr(),
				    ", offering ", dhcp.yiaddr(),
				    ", subnet-mask ", subnet_mask,
				    ", gateway ", router_ip,
			        ", DNS server ", dns_server);
			}
		}
		switch (_state) {
		case State::SELECT:

			if (msg_type.value() != Message_type::OFFER) {
				result = packet_drop("DHCP client expects an offer");
				break;
			}
			enum { REQUEST_PKT_SIZE = 321 };
			_set_state(State::REQUEST, _interface.config().dhcp_request_timeout());
			_send(Message_type::REQUEST, Ipv4_address(),
			      dhcp.option<Dhcp_packet::Server_ipv4>().value(),
			      dhcp.yiaddr(), REQUEST_PKT_SIZE);
			result = packet_handled();
			break;

		case State::REQUEST:
		case State::RENEW:
		case State::REBIND:
			{
				if (msg_type.value() != Message_type::ACK) {
					result = packet_drop("DHCP client expects an acknowledgement");
					break;
				}
				_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
				_set_state(State::BOUND, _rerequest_timeout(1, domain));
				domain.ip_config_from_dhcp_ack(dhcp);
				result = packet_handled();
				break;
			}
		default: result = packet_drop("DHCP client doesn't expect a packet"); break;
		}
	};
	dhcp.with_option<Dhcp_packet::Message_type_option>(msg_type_fn, no_msg_type_fn);
	return result;
}


void Dhcp_client::_send(Message_type msg_type,
                        Ipv4_address client_ip,
                        Ipv4_address server_ip,
                        Ipv4_address requested_ip,
                        size_t       pkt_size)
{
	Mac_address client_mac = _interface.router_mac();
	_interface.send(pkt_size, [&] (void *pkt_base, Size_guard &size_guard) {

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
		enum { MAX_PKT_SIZE = 1024 };
		Dhcp_options dhcp_opts(dhcp, size_guard);
		dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
		switch (msg_type) {
		case Message_type::DISCOVER:
			append_param_req_list(dhcp_opts);
			dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
			dhcp_opts.append_option<Dhcp_packet::Max_msg_size>(MAX_PKT_SIZE - dhcp_off);
			break;

		case Message_type::REQUEST:
			append_param_req_list(dhcp_opts);
			dhcp_opts.append_option<Dhcp_packet::Client_id>(client_mac);
			dhcp_opts.append_option<Dhcp_packet::Max_msg_size>(MAX_PKT_SIZE - dhcp_off);
			if (_state == State::REQUEST) {
				dhcp_opts.append_option<Dhcp_packet::Requested_addr>(requested_ip);
				dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(server_ip);
			}
			break;

		default: ASSERT_NEVER_REACHED;
		}
		dhcp_opts.append_option<Dhcp_packet::Options_end>();

		/* fill in header values that need the packet to be complete already */
		udp.length(size_guard.head_size() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});

	_interface.wakeup_source();
}
