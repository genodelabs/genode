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

using namespace Genode;
using namespace Net;


Ipv4_config::Ipv4_config(Allocator &alloc)
:
	alloc      { alloc },
	interface  { },
	gateway    { }
{ }


Ipv4_config::Ipv4_config(Xml_node const &domain_node,
                         Allocator      &alloc)
:
	alloc      { alloc },
	interface  { domain_node.attribute_value("interface",  Ipv4_address_prefix()) },
	gateway    { domain_node.attribute_value("gateway",    Ipv4_address()) }
{ }


Ipv4_config::Ipv4_config(Ipv4_config const &ip_config,
                         Allocator         &alloc)
:
	alloc      { alloc },
	interface  { ip_config.interface },
	gateway    { ip_config.gateway }
{
	ip_config.dns_servers.for_each([&] (Dns_server const &dns_server) {
		dns_servers.insert_as_tail(
			*new (alloc) Dns_server(dns_server.ip()));
	});
}


Ipv4_config::Ipv4_config(Dhcp_packet &dhcp_ack,
                         Allocator   &alloc)
:
	alloc      { alloc },
	interface  { dhcp_ack.yiaddr(),
	             dhcp_ipv4_option<Dhcp_packet::Subnet_mask>(dhcp_ack) },
	gateway    { dhcp_ipv4_option<Dhcp_packet::Router_ipv4>(dhcp_ack) }
{
	dhcp_ack.for_each_option([&] (Dhcp_packet::Option const &opt)
	{
		if (opt.code() != Dhcp_packet::Option::Code::DNS_SERVER) {
			return;
		}
		try {
			dns_servers.insert_as_tail(*new (alloc)
				Dns_server(
					reinterpret_cast<Dhcp_packet::Dns_server_ipv4 const *>(&opt)->value()));
		}
		catch (Dns_server::Invalid) { }
	});
}


Ipv4_config::~Ipv4_config()
{
	dns_servers.destroy_each(alloc);
}


void Ipv4_config::print(Output &output) const
{
	if (valid) {

		Genode::print(output, "interface ", interface, ", gateway ", gateway,
		              " P2P ", point_to_point);

		for_each_dns_server([&] (Dns_server const &dns_server) {
			Genode::print(output, ", DNS server ", dns_server.ip()); });

	} else {

		Genode::print(output, "none");
	}
}
