/*
 * \brief  Virtual local network.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 *
 * A database containing all clients sorted by IP and MAC addresses.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VLAN_H_
#define _VLAN_H_

#include <util/avl_tree.h>
#include <util/list.h>
#include <address_node.h>

namespace Net {

	/*
	 * The Vlan is a database containing all clients
	 * sorted by IP and MAC addresses.
	 */
	struct Vlan
	{
		using Mac_address_tree  = Genode::Avl_tree<Mac_address_node>;
		using Ipv4_address_tree = Genode::Avl_tree<Ipv4_address_node>;
		using Mac_address_list  = Genode::List<Mac_address_node>;

		Mac_address_tree  mac_tree;
		Mac_address_list  mac_list;
		Ipv4_address_tree ip_tree;
	};
}

#endif /* _VLAN_H_ */
