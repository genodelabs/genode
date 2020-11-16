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

#ifndef _IPV4_CONFIG_H_
#define _IPV4_CONFIG_H_

/* local includes */
#include <ipv4_address_prefix.h>
#include <dhcp.h>
#include <dns_server.h>

/* Genode includes */
#include <util/xml_node.h>

namespace Net { class Ipv4_config; }

struct Net::Ipv4_config
{
	Genode::Allocator           &alloc;
	Ipv4_address_prefix   const  interface;
	bool                  const  interface_valid { interface.valid() };
	Ipv4_address          const  gateway;
	bool                  const  gateway_valid   { gateway.valid() };
	bool                  const  point_to_point  { gateway_valid &&
	                                               interface_valid &&
	                                               interface.prefix == 32 };
	Net::List<Dns_server>        dns_servers     { };
	bool                  const  valid           { point_to_point ||
	                                               (interface_valid &&
	                                                (!gateway_valid ||
	                                                 interface.prefix_matches(gateway))) };

	Ipv4_config(Net::Dhcp_packet  &dhcp_ack,
	            Genode::Allocator &alloc);

	Ipv4_config(Genode::Xml_node const &domain_node,
	            Genode::Allocator      &alloc);

	Ipv4_config(Ipv4_config const &ip_config,
	            Genode::Allocator &alloc);

	Ipv4_config(Genode::Allocator &alloc);

	~Ipv4_config();

	bool operator != (Ipv4_config const &other) const
	{
		return interface  != other.interface ||
		       gateway    != other.gateway ||
		       !dns_servers.equal_to(other.dns_servers);
	}

	template <typename FUNC>
	void for_each_dns_server(FUNC && functor) const
	{
		dns_servers.for_each([&] (Dns_server const &dns_server) {
			functor(dns_server);
		});
	}


	/*********
	 ** log **
	 *********/

	void print(Genode::Output &output) const;
};

#endif /* _IPV4_CONFIG_H_ */
