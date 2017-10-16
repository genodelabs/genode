/*
 * \brief  DHCP server role of a domain
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DHCP_SERVER_H_
#define _DHCP_SERVER_H_

/* local includes */
#include <ipv4_address_prefix.h>
#include <bit_allocator_dynamic.h>

/* Genode includes */
#include <util/noncopyable.h>
#include <util/xml_node.h>
#include <os/duration.h>

namespace Net { class Dhcp_server; }


class Net::Dhcp_server : private Genode::Noncopyable
{
	private:

		Ipv4_address         const    _dns_server;
		Genode::Microseconds const    _ip_lease_time;
		Ipv4_address         const    _ip_first;
		Ipv4_address         const    _ip_last;
		Genode::uint32_t     const    _ip_first_raw;
		Genode::uint32_t     const    _ip_count;
		Genode::Bit_allocator_dynamic _ip_alloc;

		Genode::Microseconds _init_ip_lease_time(Genode::Xml_node const node);

	public:

		enum { DEFAULT_IP_LEASE_TIME_SEC = 3600 };

		struct Alloc_ip_failed : Genode::Exception { };
		struct Invalid         : Genode::Exception { };

		Dhcp_server(Genode::Xml_node    const  node,
		            Genode::Allocator         &alloc,
		            Ipv4_address_prefix const &interface);

		Ipv4_address alloc_ip();

		void free_ip(Ipv4_address const &ip);


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address   const &dns_server()    const { return _dns_server; }
		Genode::Microseconds  ip_lease_time() const { return _ip_lease_time; }
};

#endif /* _DHCP_SERVER_H_ */
