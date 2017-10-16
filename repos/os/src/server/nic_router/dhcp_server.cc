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

/* local includes */
#include <dhcp_server.h>
#include <interface.h>

using namespace Net;
using namespace Genode;


/*****************
 ** Dhcp_server **
 *****************/

Dhcp_server::Dhcp_server(Xml_node            const  node,
                         Allocator                 &alloc,
                         Ipv4_address_prefix const &interface)
:
	_dns_server(node.attribute_value("dns_server", Ipv4_address())),
	_ip_lease_time(_init_ip_lease_time(node)),
	_ip_first(node.attribute_value("ip_first", Ipv4_address())),
	_ip_last(node.attribute_value("ip_last", Ipv4_address())),
	_ip_first_raw(_ip_first.to_uint32_little_endian()),
	_ip_count(_ip_last.to_uint32_little_endian() - _ip_first_raw),
	_ip_alloc(alloc, _ip_count)
{
	if (!interface.prefix_matches(_ip_first) ||
	    !interface.prefix_matches(_ip_last) ||
	    interface.address.is_in_range(_ip_first, _ip_last))
	{
		throw Invalid();
	}
}


Microseconds Dhcp_server::_init_ip_lease_time(Xml_node const node)
{
	unsigned long ip_lease_time_sec =
		node.attribute_value("ip_lease_time_sec", 0UL);

	if (!ip_lease_time_sec) {
		warning("fall back to default ip_lease_time_sec=\"",
		        (unsigned long)DEFAULT_IP_LEASE_TIME_SEC, "\"");
		ip_lease_time_sec = DEFAULT_IP_LEASE_TIME_SEC;
	}
	return Microseconds((unsigned long)ip_lease_time_sec * 1000 * 1000);
}


void Dhcp_server::print(Output &output) const
{
	if (_dns_server.valid()) {
		Genode::print(output, "DNS server ", _dns_server, " ");
	}
	Genode::print(output, "IP first ", _ip_first,
	                        ", last ", _ip_last,
	                       ", count ", _ip_count,
	                  ", lease time ", _ip_lease_time.value / 1000 / 1000, " sec");
}


Ipv4_address Dhcp_server::alloc_ip()
{
	try {
		return Ipv4_address::from_uint32_little_endian(_ip_alloc.alloc() +
		                                               _ip_first_raw);
	}
	catch (Bit_allocator_dynamic::Out_of_indices) {
		throw Alloc_ip_failed();
	}
}


void Dhcp_server::free_ip(Ipv4_address const &ip)
{
	_ip_alloc.free(ip.to_uint32_little_endian() - _ip_first_raw);
}


/*********************
 ** Dhcp_allocation **
 *********************/

Dhcp_allocation::Dhcp_allocation(Interface      &interface,
                             Ipv4_address const &ip,
                             Mac_address  const &mac,
                             Timer::Connection  &timer,
                             Microseconds        lifetime)
:
	_interface(interface), _ip(ip), _mac(mac),
	_timeout(timer, *this, &Dhcp_allocation::_handle_timeout)
{
	_timeout.schedule(lifetime);
}


void Dhcp_allocation::lifetime(Microseconds lifetime)
{
	_timeout.schedule(lifetime);
}


bool Dhcp_allocation::_higher(Mac_address const &mac) const
{
	return memcmp(mac.addr, _mac.addr, sizeof(_mac.addr)) > 0;
}


Dhcp_allocation &Dhcp_allocation::find_by_mac(Mac_address const &mac)
{
	if (mac == _mac) {
		return *this; }

	Dhcp_allocation *const allocation = child(_higher(mac));
	if (!allocation) {
		throw Dhcp_allocation_tree::No_match(); }

	return allocation->find_by_mac(mac);
}


void Dhcp_allocation::print(Output &output) const
{
	Genode::print(output, "MAC ", _mac, " IP ", _ip);
}


void Dhcp_allocation::_handle_timeout(Duration)
{
	_interface.dhcp_allocation_expired(*this);
}


/**************************
 ** Dhcp_allocation_tree **
 **************************/

Dhcp_allocation &
Dhcp_allocation_tree::find_by_mac(Mac_address const &mac) const
{
	if (!first()) {
		throw No_match(); }

	return first()->find_by_mac(mac);
}
