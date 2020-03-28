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
using Drop_packet  = Net::Interface::Drop_packet;
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

Configuration &Dhcp_client::_config() { return _domain().config(); };


Domain &Dhcp_client::_domain() { return _interface.domain(); }


Dhcp_client::Dhcp_client(Cached_timer      &timer,
                         Interface         &interface)
:
	_interface(interface),
	_timeout(timer, *this, &Dhcp_client::_handle_timeout)
{ }


void Dhcp_client::discover()
{
	enum { DISCOVER_PKT_SIZE = 309 };
	_set_state(State::SELECT, _config().dhcp_discover_timeout());
	_send(Message_type::DISCOVER, Ipv4_address(), Ipv4_address(),
	      Ipv4_address(), DISCOVER_PKT_SIZE);
}


void Dhcp_client::_rerequest(State next_state)
{
	enum { REREQUEST_PKT_SIZE = 309 };
	_set_state(next_state, _rerequest_timeout(2));
	Ipv4_address const client_ip = _domain().ip_config().interface().address;
	_send(Message_type::REQUEST, client_ip, Ipv4_address(), client_ip,
	      REREQUEST_PKT_SIZE);
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
		if (_interface.config().verbose()) {
			try {
				log("[", _interface.domain(), "] prune re-request timeout of "
				    "DHCP client");
			}
			catch (Pointer<Domain>::Invalid) {
				log("[?] prune re-request timeout of DHCP client"); }
		}
	}
	return Microseconds(timeout_sec * 1000 * 1000);
}


void Dhcp_client::_handle_timeout(Duration)
{
	switch (_state) {
	case State::BOUND:  _rerequest(State::RENEW);      break;
	case State::RENEW:  _rerequest(State::REBIND);     break;
	case State::REBIND: _domain().discard_ip_config(); [[fallthrough]];
	default:            discover();
	}
}


void Dhcp_client::handle_dhcp_reply(Dhcp_packet &dhcp)
{
	try {
		Message_type const msg_type =
			dhcp.option<Dhcp_packet::Message_type_option>().value();

		if (_interface.config().verbose_domain_state()) {
			if (msg_type == Message_type::OFFER) {
				Ipv4_address dns_server;
				Ipv4_address subnet_mask;
				Ipv4_address router_ip;

				try { dns_server = dhcp.option<Dhcp_packet::Dns_server_ipv4>().value(); }
				catch (Dhcp_packet::Option_not_found) { }
				try { subnet_mask = dhcp.option<Dhcp_packet::Subnet_mask>().value(); }
				catch (Dhcp_packet::Option_not_found) { }
				try { router_ip = dhcp.option<Dhcp_packet::Router_ipv4>().value(); }
				catch (Net::Dhcp_packet::Option_not_found) { }

				log("[", _interface.domain(), "] dhcp offer from ",
				    dhcp.siaddr(),
				    ", offering ", dhcp.yiaddr(),
				    ", subnet-mask ", subnet_mask,
				    ", gateway ", router_ip,
			        ", DNS server ", dns_server);
			}
		}

		switch (_state) {
		case State::SELECT:

			if (msg_type != Message_type::OFFER) {
				throw Drop_packet("DHCP client expects an offer");
			}
			enum { REQUEST_PKT_SIZE = 321 };
			_set_state(State::REQUEST, _config().dhcp_request_timeout());
			_send(Message_type::REQUEST, Ipv4_address(),
			      dhcp.option<Dhcp_packet::Server_ipv4>().value(),
			      dhcp.yiaddr(), REQUEST_PKT_SIZE);
			break;

		case State::REQUEST:
		case State::RENEW:
		case State::REBIND:
			{
				if (msg_type != Message_type::ACK) {
					throw Drop_packet("DHCP client expects an acknowledgement");
				}
				_lease_time_sec = dhcp.option<Dhcp_packet::Ip_lease_time>().value();
				_set_state(State::BOUND, _rerequest_timeout(1));
				_domain().ip_config_from_dhcp_ack(dhcp);
				break;
			}
		default: throw Drop_packet("DHCP client doesn't expect a packet");
		}
	}
	catch (Dhcp_packet::Option_not_found) {
		throw Drop_packet("DHCP reply misses required option");
	}
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

		default:
			throw Interface::Bad_send_dhcp_args();
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
