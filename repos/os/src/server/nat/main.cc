/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <nic/xml_node.h>
#include <os/server.h>

/* local includes */
#include <component.h>
#include <arp_cache.h>
#include <uplink.h>
#include <port_allocator.h>

using namespace Net;
using namespace Genode;


class Main
{
	private:

		bool const          _verbose;
		Port_allocator      _tcp_port_alloc;
		Port_allocator      _udp_port_alloc;
		Server::Entrypoint &_ep;
		Interface_tree      _interface_tree;
		Arp_cache           _arp_cache;
		Arp_waiter_list     _arp_waiters;
		Tcp_proxy_list      _tcp_proxys;
		Udp_proxy_list      _udp_proxys;
		unsigned            _rtt_sec;
		Uplink              _uplink;
		Net::Root           _root;

		void _read_ports(Genode::Xml_node &route, char const *name);

	public:

		Main(Server::Entrypoint &ep);
};


void Main::_read_ports(Xml_node &route, char const *name)
{
	try {
		for (Xml_node port = route.sub_node(name); ; port = port.next(name)) {
			uint16_t const dst = port.attribute_value("dst", 0UL);
			if (!dst) {
				warning("missing 'dst' attribute in port route");
				continue;
			}
			if (_verbose) {
				log("Reserve ", name, " ", dst); }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Main::Main(Server::Entrypoint &ep)
:
	_verbose(config()->xml_node().attribute_value("verbose", false)),
	_ep(ep), _rtt_sec(config()->xml_node().attribute_value("rtt_sec", 0UL)),

	_uplink(_ep, _tcp_port_alloc, _udp_port_alloc, _tcp_proxys,
	        _udp_proxys, _rtt_sec, _interface_tree, _arp_cache,
	        _arp_waiters, _verbose),

	_root(_ep, *env()->heap(), _uplink.nat_mac(), _tcp_port_alloc,
	      _udp_port_alloc, _tcp_proxys, _udp_proxys,
	      _rtt_sec, _interface_tree, _arp_cache, _arp_waiters, _verbose)
{
	if (!_rtt_sec) { warning("missing 'rtt_sec' attribute in config tag"); }

	/* reserve all ports that are used in port routes */
	try {
		Xml_node policy = config()->xml_node().sub_node("policy");
		for (; ; policy = policy.next("policy")) {
			try {
				Xml_node route = policy.sub_node("ip");
				for (; ; route = route.next("ip")) {
					_read_ports(route, "tcp");
					_read_ports(route, "udp");
				}
			} catch (Xml_node::Nonexistent_sub_node) { }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }

	/* announce service */
	env()->parent()->announce(ep.manage(_root));
}


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nat_ep"; }

	size_t stack_size() { return 4096 *sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Main nat(ep); }
}
