/*
* \brief  Cache for received ARP information
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
#include <arp_cache.h>
#include <domain.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


/*********************
 ** Arp_cache_entry **
 *********************/

Arp_cache_entry::Arp_cache_entry(Ipv4_address const &ip,
                                 Mac_address  const &mac)
:
	_ip(ip), _mac(mac)
{ }


bool Arp_cache_entry::_higher(Ipv4_address const &ip) const
{
	return memcmp(ip.addr, _ip.addr, sizeof(_ip.addr)) > 0;
}


void Arp_cache_entry::print(Output &output) const
{
	Genode::print(output, _ip, " > ", _mac);
}


/***************
 ** Arp_cache **
 ***************/

void Arp_cache::new_entry(Ipv4_address const &ip, Mac_address const &mac)
{
	if (_entries[_curr].constructed()) {
		remove(&(*_entries[_curr]));
	}
	_entries[_curr].construct(ip, mac);
	Arp_cache_entry &entry = *_entries[_curr];
	insert(&entry);
	if (_domain.config().verbose()) {
		log("[", _domain, "] new ARP entry ", entry);
	}
	if (_curr < NR_OF_ENTRIES - 1) {
		_curr++;
	} else {
		_curr = 0;
	};
}


void Arp_cache::destroy_entries_with_mac(Mac_address const &mac)
{
	for (unsigned curr = 0; curr < NR_OF_ENTRIES; curr++) {
		try {
			Arp_cache_entry &entry = *_entries[curr];
			if (entry.mac() != mac) {
				continue;
			}
			if (_domain.config().verbose()) {
				log("[", _domain, "] destroy ARP entry ", entry);
			}
			remove(&entry);
			_entries[curr].destruct();

		} catch (Arp_cache_entry_slot::Deref_unconstructed_object) { }
	}
}


void Arp_cache::destroy_all_entries()
{
	if (_domain.config().verbose()) {
		log("[", _domain, "] destroy all ARP entries");
	}
	while (Arp_cache_entry *entry = first()) {
		remove(entry);
	}
	for (unsigned curr = 0; curr < NR_OF_ENTRIES; curr++) {
		_entries[curr].destruct();
	}
}
