/*
 * \brief  Port routing entry
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
#include <base/session_label.h>
#include <util/avl_tree.h>
#include <util/list.h>
#include <net/ipv4.h>

#ifndef _PORT_ROUTE_H_
#define _PORT_ROUTE_H_

namespace Net {

	class Port_route;
	class Port_route_tree;
	using Port_route_list = Genode::List<Port_route>;
}


class Net::Port_route : public Genode::Avl_node<Port_route>,
                        public Port_route_list::Element
{
	private:

		Genode::uint16_t      _dst;
		Genode::Session_label _label;
		Ipv4_address          _via;
		Ipv4_address          _to;

	public:

		Port_route(Genode::uint16_t dst, char const *label,
		           Genode::size_t label_size, Ipv4_address via,
		           Ipv4_address to);

		void print(Genode::Output &output) const;

		Port_route *find_by_dst(Genode::uint16_t dst);


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Port_route *route) { return route->_dst > _dst; }


		/***************
		 ** Accessors **
		 ***************/

		Genode::Session_label &label() { return _label; }
		Ipv4_address           via()   { return _via; }
		Ipv4_address           to()    { return _to; }
		Genode::uint16_t       dst()   { return _dst; }
};


struct Net::Port_route_tree : Genode::Avl_tree<Port_route>
{
	Port_route *find_by_dst(Genode::uint16_t dst);
};

#endif /* _PORT_ROUTE_H_ */
