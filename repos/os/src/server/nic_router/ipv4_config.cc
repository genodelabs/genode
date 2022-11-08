/*
 * \brief  IPv4 peer configuration
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <ipv4_config.h>
#include <domain.h>
#include <configuration.h>

using namespace Genode;
using namespace Net;


Ipv4_config::Ipv4_config(Allocator &alloc)
:
	_alloc      { alloc },
	_interface  { },
	_gateway    { }
{ }


Ipv4_config::Ipv4_config(Xml_node const &domain_node,
                         Allocator      &alloc)
:
	_alloc      { alloc },
	_interface  { domain_node.attribute_value("interface",  Ipv4_address_prefix()) },
	_gateway    { domain_node.attribute_value("gateway",    Ipv4_address()) }
{ }


Ipv4_config::Ipv4_config(Ipv4_config const &ip_config,
                         Allocator         &alloc)
:
	_alloc      { alloc },
	_interface  { ip_config._interface },
	_gateway    { ip_config._gateway }
{
	ip_config.for_each_dns_server([&] (Dns_server const &dns_server) {
		Dns_server::construct(
			_alloc, dns_server.ip(),
			[&] /* handle_success */ (Dns_server &server)
			{
				_dns_servers.insert_as_tail(server);
			},
			[&] /* handle_failure */ () { }
		);
	});
	_dns_domain_name.set_to(ip_config.dns_domain_name());
}


Ipv4_config::Ipv4_config(Ipv4_config const &ip_config)
:
	_alloc      { ip_config._alloc },
	_interface  { ip_config._interface },
	_gateway    { ip_config._gateway }
{
	ip_config.for_each_dns_server([&] (Dns_server const &dns_server) {
		Dns_server::construct(
			_alloc, dns_server.ip(),
			[&] /* handle_success */ (Dns_server &server)
			{
				_dns_servers.insert_as_tail(server);
			},
			[&] /* handle_failure */ () { }
		);
	});
	_dns_domain_name.set_to(ip_config.dns_domain_name());
}


Ipv4_config::Ipv4_config(Dhcp_packet  &dhcp_ack,
                         Allocator    &alloc,
                         Domain const &domain)
:
	_alloc      { alloc },
	_interface  { dhcp_ack.yiaddr(),
	              dhcp_ipv4_option<Dhcp_packet::Subnet_mask>(dhcp_ack) },
	_gateway    { dhcp_ipv4_option<Dhcp_packet::Router_ipv4>(dhcp_ack) }
{
	try {
		Dhcp_packet::Dns_server const &dns_server {
			dhcp_ack.option<Dhcp_packet::Dns_server>() };

		dns_server.for_each_address([&] (Ipv4_address const &addr) {
			Dns_server::construct(
				alloc, addr,
				[&] /* handle_success */ (Dns_server &server)
				{
					_dns_servers.insert_as_tail(server);
				},
				[&] /* handle_failure */ () { }
			);
		});
	}
	catch (Dhcp_packet::Option_not_found) { }
	try {
		_dns_domain_name.set_to(dhcp_ack.option<Dhcp_packet::Domain_name>());

		if (domain.config().verbose() &&
		    !_dns_domain_name.valid()) {

			log("[", domain, "] rejecting oversized DNS "
			    "domain name from DHCP reply");
		}
	}
	catch (Dhcp_packet::Option_not_found) { }
}


Ipv4_config::~Ipv4_config()
{
	_dns_servers.destroy_each(_alloc);
}


void Ipv4_config::print(Output &output) const
{
	if (_valid) {

		Genode::print(output, "interface ", _interface, ", gateway ", _gateway,
		              ", P2P ", _point_to_point);

		for_each_dns_server([&] (Dns_server const &dns_server) {
			Genode::print(output, ", DNS server ", dns_server.ip()); });

	} else if (_interface_valid || _gateway_valid || !_dns_servers.empty()) {

		Genode::print(output, "malformed (interface ", _interface,
		              ", gateway ", _gateway, ", P2P ", _point_to_point);

		for_each_dns_server([&] (Dns_server const &dns_server) {
			Genode::print(output, ", DNS server ", dns_server.ip()); });

		Genode::print(output, ")");

	} else {

		Genode::print(output, "none");
	}
}
