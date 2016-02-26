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

#include <address_node.h>
#include <avl_safe.h>
#include <list_safe.h>

namespace Net {

	/*
	 * The Vlan is a database containing all clients
	 * sorted by IP and MAC addresses.
	 */
	class Vlan
	{
		public:

			typedef Avl_tree_safe<Mac_address_node>  Mac_address_tree;
			typedef Avl_tree_safe<Ipv4_address_node> Ipv4_address_tree;
			typedef List_safe<Mac_address_node>      Mac_address_list;

		private:

			Mac_address_tree  _mac_tree;
			Mac_address_list  _mac_list;
			Ipv4_address_tree _ip_tree;

		public:

			Vlan() {}

			Mac_address_tree  *mac_tree() { return &_mac_tree; }
			Mac_address_list  *mac_list() { return &_mac_list; }
			Ipv4_address_tree *ip_tree()  { return &_ip_tree;  }
	};
}

#endif /* _VLAN_H_ */
