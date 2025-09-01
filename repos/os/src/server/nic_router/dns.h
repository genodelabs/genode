/*
 * \brief  Utilities for handling DNS configurations
 * \author Martin Stein
 * \date   2020-11-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DNS_H_
#define _DNS_H_

/* local includes */
#include <list.h>

/* Genode includes */
#include <util/reconstructible.h>
#include <net/ipv4.h>
#include <net/dhcp.h>
#include <base/node.h>

namespace Net {

	class Dns_server;
	class Dns_domain_name;

	using Dns_server_list = Net::List<Dns_server>;
}

class Net::Dns_server : private Genode::Noncopyable,
                        public  Net::List<Dns_server>::Element
{
	private:

		Net::Ipv4_address const _ip;

		Dns_server(Net::Ipv4_address const &ip);

	public:

		static void construct(Genode::Allocator       &alloc,
		                      Net::Ipv4_address const &ip,
		                      auto              const &handle_success,
		                      auto              const &handle_failure)
		{
			if (!ip.valid()) {
				handle_failure();
			}
			handle_success(*new (alloc) Dns_server(ip));
		}

		bool equal_to(Dns_server const &server) const;


		/***************
		 ** Accessors **
		 ***************/

		Net::Ipv4_address const &ip() const { return _ip; }
};

class Net::Dns_domain_name
{
	public:

		/* max. 253 ASCII characters + terminating 0 + oversize detection byte */
		using String = Genode::String<253+1+1>;

	private:

		String _string { };

		static bool _string_valid(String const &s)
		{
			return s.length() > 1 && s.length() < String::capacity();
		}

	public:

		void set_to(String const &name);

		void set_to(Dhcp_packet::Domain_name const &name_option);

		bool valid() const { return _string_valid(_string); }

		void with_string(auto const &func) const
		{
			if (_string.valid())
				func(_string);
		}

		bool equal_to(Dns_domain_name const &other) const;
};

#endif /* _DNS_H_ */
