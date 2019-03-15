/*
 * \brief  Address-node holds a client-specific session-component.
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ADDRESS_NODE_H_
#define _ADDRESS_NODE_H_

/* Genode */
#include <util/avl_tree.h>
#include <util/list.h>
#include <nic_session/nic_session.h>
#include <net/netaddress.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net {

	/* Forward declaration */
	class Session_component;


	/**
	 * An Address_node encapsulates a session-component and can be hold in
	 * a list and/or avl-tree, whereby the network-address (MAC or IP)
	 * acts as a key.
	 */
	template <typename ADDRESS> class Address_node;

	using Ipv4_address_node = Address_node<Ipv4_address>;
	using Mac_address_node  = Address_node<Mac_address>;
}


template <typename ADDRESS>
class Net::Address_node : public Genode::Avl_node<Address_node<ADDRESS> >,
                          public Genode::List<Address_node<ADDRESS> >::Element
{
	private:

		ADDRESS            _addr;       /* MAC or IP address  */
		Session_component &_component;  /* client's component */

	public:

		using Address = ADDRESS;

		/**
		 * Constructor
		 *
		 * \param component  reference to client's session component.
		 */
		Address_node(Session_component &component,
		             Address addr = Address())
		: _addr(addr), _component(component) { }


		/***************
		 ** Accessors **
		 ***************/

		void               addr(Address addr) { _addr = addr;      }
		Address            addr()       const { return _addr;      }
		Session_component &component()        { return _component; }


		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Address_node *c)
		{
			using namespace Genode;

			return (memcmp(&c->_addr.addr, &_addr.addr,
			               sizeof(_addr.addr)) > 0);
		}

		/**
		 * Find by address
		 */
		Address_node *find_by_address(Address addr)
		{
			using namespace Genode;

			if (addr == _addr)
				return this;

			bool side = memcmp(&addr.addr, _addr.addr,
			                   sizeof(_addr.addr)) > 0;
			Address_node *c = Avl_node<Address_node>::child(side);
			return c ? c->find_by_address(addr) : 0;
		}
};

#endif /* _ADDRESS_NODE_H_ */
