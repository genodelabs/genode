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


unsigned read_rtt_sec()
{
	unsigned rtt_sec = config()->xml_node().attribute_value("rtt_sec", 0UL);
	if (!rtt_sec) {
		rtt_sec = 3;
		warning("missing 'rtt_sec' attribute in config tag,",
		        "using default value \"3\"");
	}
	return rtt_sec;
}


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

		void _read_ports(Genode::Xml_node &route, char const *name,
		                 Port_allocator &_port_alloc);


	public:

		Main(Server::Entrypoint &ep);
};


void Main::_read_ports(Xml_node &route, char const *name,
                       Port_allocator &port_alloc)
{
	try {
		for (Xml_node port = route.sub_node(name); ; port = port.next(name)) {
			uint32_t const dst = port.attribute_value("dst", 0UL);
			if (!dst) {
				warning("missing 'dst' attribute in port route");
				continue;
			}
			if (dst >= Port_allocator::FIRST &&
			    dst <  Port_allocator::FIRST + Port_allocator::COUNT)
			{
				error("port forwarding clashes with dynamic port range"); }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Main::Main(Server::Entrypoint &ep)
:
	_verbose(config()->xml_node().attribute_value("verbose", false)),
	_ep(ep), _rtt_sec(read_rtt_sec()),

	_uplink(_ep, _tcp_port_alloc, _udp_port_alloc, _tcp_proxys,
	        _udp_proxys, _rtt_sec, _interface_tree, _arp_cache,
	        _arp_waiters, _verbose),

	_root(_ep, *env()->heap(), _uplink.router_mac(), _tcp_port_alloc,
	      _udp_port_alloc, _tcp_proxys, _udp_proxys,
	      _rtt_sec, _interface_tree, _arp_cache, _arp_waiters, _verbose)
{
	/* reserve all ports that are used in port routes */
	try {
		Xml_node policy = config()->xml_node().sub_node("policy");
		for (; ; policy = policy.next("policy")) {
			try {
				Xml_node route = policy.sub_node("ip");
				for (; ; route = route.next("ip")) {
					_read_ports(route, "tcp", _tcp_port_alloc);
					_read_ports(route, "udp", _udp_port_alloc);
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

	char const *name() { return "nic_router_ep"; }

	size_t stack_size() { return 4096 *sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Main router(ep); }
}
