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

#ifndef _ARP_CACHE_H_
#define _ARP_CACHE_H_

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <util/avl_tree.h>
#include <util/reconstructible.h>

namespace Net {

	class Domain;
	class Arp_cache;
	class Arp_cache_entry;
	using Arp_cache_entry_slot = Genode::Constructible<Arp_cache_entry>;
}


class Net::Arp_cache_entry : public Genode::Avl_node<Arp_cache_entry>
{
	private:

		Ipv4_address const _ip;
		Mac_address  const _mac;

		bool _higher(Ipv4_address const &ip) const;

	public:

		Arp_cache_entry(Ipv4_address const &ip, Mac_address const &mac);

		Arp_cache_entry const &find_by_ip(Ipv4_address const &ip) const;


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Arp_cache_entry *entry) { return _higher(entry->_ip); }


		/***************
		 ** Accessors **
		 ***************/

		Mac_address const &mac() const { return _mac; }
		Ipv4_address const &ip() const { return _ip; }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};


class Net::Arp_cache : public Genode::Avl_tree<Arp_cache_entry>
{
	private:

		enum {
			ENTRIES_SIZE  = 1024 * sizeof(Genode::addr_t),
			NR_OF_ENTRIES = ENTRIES_SIZE / sizeof(Arp_cache_entry),
		};

		Domain const         &_domain;
		Arp_cache_entry_slot  _entries[NR_OF_ENTRIES];
		bool                  _init = true;
		unsigned              _curr = 0;

	public:

		struct No_match : Genode::Exception { };

		Arp_cache(Domain const &domain) : _domain(domain) { }

		void new_entry(Ipv4_address const &ip, Mac_address const &mac);

		void destroy_entries_with_mac(Mac_address const &mac);

		Arp_cache_entry const &find_by_ip(Ipv4_address const &ip) const;
};

#endif /* _ARP_CACHE_H_ */
