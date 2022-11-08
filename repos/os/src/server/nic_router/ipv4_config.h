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
#include <dns.h>

/* Genode includes */
#include <util/xml_node.h>

namespace Net {

	class Domain;
	class Ipv4_config;
}

class Net::Ipv4_config
{
	private:

		Genode::Allocator           &_alloc;
		Ipv4_address_prefix   const  _interface;
		bool                  const  _interface_valid { _interface.valid() };
		Ipv4_address          const  _gateway;
		bool                  const  _gateway_valid   { _gateway.valid() };
		bool                  const  _point_to_point  { _gateway_valid &&
		                                               _interface_valid &&
		                                               _interface.prefix == 32 };
		Dns_server_list              _dns_servers     { };
		Dns_domain_name              _dns_domain_name { _alloc };
		bool                  const  _valid           { _point_to_point ||
		                                               (_interface_valid &&
		                                                (!_gateway_valid ||
		                                                 _interface.prefix_matches(_gateway))) };

	public:

		Ipv4_config(Net::Dhcp_packet  &dhcp_ack,
		            Genode::Allocator &alloc,
		            Domain      const &domain);

		Ipv4_config(Genode::Xml_node const &domain_node,
		            Genode::Allocator      &alloc);

		Ipv4_config(Ipv4_config const &ip_config,
		            Genode::Allocator &alloc);

		Ipv4_config(Ipv4_config const &ip_config);

		Ipv4_config(Genode::Allocator &alloc);

		~Ipv4_config();

		bool operator != (Ipv4_config const &other) const
		{
			return _interface != other._interface             ||
			       _gateway   != other._gateway               ||
			       !_dns_servers.equal_to(other._dns_servers) ||
			       !_dns_domain_name.equal_to(other._dns_domain_name);
		}

		bool operator == (Ipv4_config const &other) const
		{
			return !(*this != other);
		}

		template <typename FUNC>
		void for_each_dns_server(FUNC && func) const
		{
			_dns_servers.for_each([&] (Dns_server const &dns_server) {
				func(dns_server);
			});
		}


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		bool                       valid()             const { return _valid; }
		Ipv4_address_prefix const &interface()         const { return _interface; }
		Ipv4_address        const &gateway()           const { return _gateway; }
		bool                       gateway_valid()     const { return _gateway_valid; }
		Dns_domain_name     const &dns_domain_name()   const { return _dns_domain_name; }
		bool                       dns_servers_empty() const { return _dns_servers.empty(); }
};

#endif /* _IPV4_CONFIG_H_ */
