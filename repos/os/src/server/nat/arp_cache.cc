/*
* \brief  Cache for received ARP information
* \author Martin Stein
* \date   2016-08-19
*/

/*
* Copyright (C) 2016 Genode Labs GmbH
*
* This file is part of the Genode OS framework, which is distributed
* under the terms of the GNU General Public License version 2.
*/

/* local includes */
#include <arp_cache.h>

using namespace Net;
using namespace Genode;


Arp_cache_entry::Arp_cache_entry(Ipv4_address ip_addr, Mac_address mac_addr)
:
	_ip_addr(ip_addr), _mac_addr(mac_addr)
{ }


bool Arp_cache_entry::higher(Arp_cache_entry *entry)
{
	return (memcmp(&entry->_ip_addr.addr, &_ip_addr.addr,
	               sizeof(_ip_addr.addr)) > 0);
}


Arp_cache_entry &Arp_cache_entry::find_by_ip_addr(Ipv4_address ip_addr)
{
	if (ip_addr == _ip_addr) {
		return *this; }

	bool const side =
		memcmp(&ip_addr.addr, _ip_addr.addr, sizeof(_ip_addr.addr)) > 0;

	Arp_cache_entry * entry = Avl_node<Arp_cache_entry>::child(side);
	if (!entry) {
		throw Arp_cache::No_matching_entry(); }

	return entry->find_by_ip_addr(ip_addr);
}


Arp_cache_entry &Arp_cache::find_by_ip_addr(Ipv4_address ip_addr)
{
	Arp_cache_entry * const entry = first();
	if (!entry) {
		throw No_matching_entry(); }

	return entry->find_by_ip_addr(ip_addr);
}
