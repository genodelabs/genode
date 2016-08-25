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

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <util/avl_tree.h>

#ifndef _ARP_CACHE_H_
#define _ARP_CACHE_H_

namespace Net {

	class Arp_cache;
	class Arp_cache_entry;
}

class Net::Arp_cache_entry : public Genode::Avl_node<Arp_cache_entry>
{
	private:

		Ipv4_address const _ip_addr;
		Mac_address const  _mac_addr;

	public:

		Arp_cache_entry(Ipv4_address ip_addr, Mac_address mac_addr);

		Arp_cache_entry &find_by_ip_addr(Ipv4_address ip_addr);


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Arp_cache_entry *entry);


		/***************
		 ** Accessors **
		 ***************/

		Ipv4_address ip_addr()  const { return _ip_addr; }
		Mac_address  mac_addr() const { return _mac_addr; }
};

struct Net::Arp_cache : Genode::Avl_tree<Arp_cache_entry>
{
	struct No_matching_entry : Genode::Exception { };

	Arp_cache_entry &find_by_ip_addr(Ipv4_address ip_addr);
};

#endif /* _ARP_CACHE_H_ */
