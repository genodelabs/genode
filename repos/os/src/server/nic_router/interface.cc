/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/dump.h>

/* local includes */
#include <interface.h>
#include <port_allocator.h>
#include <proxy.h>
#include <arp_cache.h>
#include <arp_waiter.h>

using namespace Net;
using namespace Genode;


static void tlp_update_checksum(uint8_t tlp, void *ptr, Ipv4_address src,
                                Ipv4_address dst, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID:
		((Tcp_packet *)ptr)->update_checksum(src, dst, size);
		return;
	case Udp_packet::IP_ID:
		((Udp_packet *)ptr)->update_checksum(src, dst);
		return;
	default: error("unknown transport protocol"); }
}


static uint16_t tlp_dst_port(uint8_t tlp, void *ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->dst_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->dst_port();
	default: error("unknown transport protocol"); }
	return 0;
}


static void tlp_dst_port(uint8_t tlp, void *ptr, uint16_t port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: ((Tcp_packet *)ptr)->dst_port(port); return;
	case Udp_packet::IP_ID: ((Udp_packet *)ptr)->dst_port(port); return;
	default: error("unknown transport protocol"); }
}


static uint16_t tlp_src_port(uint8_t tlp, void *ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->src_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->src_port();
	default: error("unknown transport protocol"); }
	return 0;
}


static void *tlp_packet(uint8_t tlp, Ipv4_packet *ip, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return new (ip->data<void>()) Tcp_packet(size);
	case Udp_packet::IP_ID: return new (ip->data<void>()) Udp_packet(size);
	default: error("unknown transport protocol"); }
	return nullptr;
}


static Port_route_tree *tlp_port_tree(uint8_t tlp, Ip_route *route)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return route->tcp_port_tree();
	case Udp_packet::IP_ID: return route->udp_port_tree();
	default: error("unknown transport protocol"); }
	return nullptr;
}


static Port_route_list *tlp_port_list(uint8_t tlp, Ip_route *route)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return route->tcp_port_list();
	case Udp_packet::IP_ID: return route->udp_port_list();
	default: error("unknown transport protocol"); }
	return nullptr;
}


void Interface::_tlp_apply_port_proxy(uint8_t tlp, void *ptr, Ipv4_packet *ip,
                                      Ipv4_address client_ip,
                                      uint16_t client_port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID:
		{
			Tcp_packet *tcp = (Tcp_packet *)ptr;
			Tcp_proxy *proxy =
				_find_tcp_proxy_by_client(client_ip, client_port);

			if (!proxy) {
				proxy = _new_tcp_proxy(client_port, client_ip, ip->src()); }

			proxy->tcp_packet(ip, tcp);
			tcp->src_port(proxy->proxy_port());
			return;
		}
	case Udp_packet::IP_ID:
		{
			Udp_packet *udp = (Udp_packet *)ptr;
			Udp_proxy *proxy =
				_find_udp_proxy_by_client(client_ip, client_port);

			if (!proxy) {
				proxy = _new_udp_proxy(client_port, client_ip, ip->src()); }

			proxy->udp_packet(ip, udp);
			udp->src_port(proxy->proxy_port());
			return;
		}
	default: error("unknown transport protocol"); }
}


Interface *Interface::_tlp_proxy_route(uint8_t tlp, void *ptr,
                                       uint16_t &dst_port, Ipv4_packet *ip,
                                       Ipv4_address &to, Ipv4_address &via)
{
	switch (tlp) {
	case Tcp_packet::IP_ID:
		{
			Tcp_packet *tcp = (Tcp_packet *)ptr;
			Tcp_proxy *proxy = _find_tcp_proxy_by_proxy(ip->dst(), dst_port);
			if (!proxy) {
				return nullptr; }

			proxy->tcp_packet(ip, tcp);
			dst_port = proxy->client_port();
			to = proxy->client_ip();
			via = to;
			if(_verbose) {
				log("Matching TCP NAT link: ", *proxy); }

			return &proxy->client();
		}
	case Udp_packet::IP_ID:
		{
			Udp_packet *udp = (Udp_packet *)ptr;
			Udp_proxy *proxy = _find_udp_proxy_by_proxy(ip->dst(), dst_port);
			if (!proxy) {
				return nullptr; }

			proxy->udp_packet(ip, udp);
			dst_port = proxy->client_port();
			to = proxy->client_ip();
			via = to;
			if (_verbose) {
				log("Matching UDP NAT link: ", *proxy); }

			return &proxy->client();
		}
	default: error("unknown transport protocol"); }
	return nullptr;
}


void Interface::_delete_tcp_proxy(Tcp_proxy * const proxy)
{
	_tcp_proxies.remove(proxy);
	unsigned const proxy_port = proxy->proxy_port();
	destroy(_allocator, proxy);
	_tcp_port_alloc.free(proxy_port);
	_tcp_proxy_used--;
	if (_verbose) {
		log("Delete TCP NAT link: ", *proxy); }
}


void Interface::_delete_udp_proxy(Udp_proxy * const proxy)
{
	_udp_proxies.remove(proxy);
	unsigned const proxy_port = proxy->proxy_port();
	destroy(_allocator, proxy);
	_udp_port_alloc.free(proxy_port);
	_udp_proxy_used--;
	if (_verbose) {
		log("Delete UDP NAT link: ", *proxy); }
}


Tcp_proxy *Interface::_new_tcp_proxy(unsigned const client_port,
                                     Ipv4_address client_ip,
                                     Ipv4_address proxy_ip)
{
	if (_tcp_proxy_used == _tcp_proxy) {
		throw Too_many_tcp_proxies(); }

	unsigned const proxy_port = _tcp_port_alloc.alloc();
	Tcp_proxy * const proxy =
		new (_allocator) Tcp_proxy(client_port, proxy_port, client_ip,
		                           proxy_ip, *this, _ep, _rtt_sec);
	_tcp_proxies.insert(proxy);
	_tcp_proxy_used++;
	if (_verbose) {
		log("New TCP NAT link: ", *proxy); }

	return proxy;
}


Udp_proxy *Interface::_new_udp_proxy(unsigned const client_port,
                                     Ipv4_address client_ip,
                                     Ipv4_address proxy_ip)
{
	if (_udp_proxy_used == _udp_proxy) {
		throw Too_many_udp_proxies(); }

	unsigned const proxy_port = _udp_port_alloc.alloc();
	Udp_proxy * const proxy =
		new (_allocator) Udp_proxy(client_port, proxy_port, client_ip,
		                           proxy_ip, *this, _ep, _rtt_sec);
	_udp_proxies.insert(proxy);
	_udp_proxy_used++;
	if (_verbose) {
		log("New UDP NAT link: ", *proxy); }

	return proxy;
}


bool Interface::_chk_delete_tcp_proxy(Tcp_proxy * &proxy)
{
	if (!proxy->del()) {
		return false; }

	Tcp_proxy * const next_proxy = proxy->next();
	_delete_tcp_proxy(proxy);
	proxy = next_proxy;
	return true;
}


bool Interface::_chk_delete_udp_proxy(Udp_proxy * &proxy)
{
	if (!proxy->del()) {
		return false; }

	Udp_proxy * const next_proxy = proxy->next();
	_delete_udp_proxy(proxy);
	proxy = next_proxy;
	return true;
}


void Interface::_handle_ip(Ethernet_frame *eth, Genode::size_t eth_size,
                           bool &ack_packet, Packet_descriptor *packet)
{
	/* prepare routing information */
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet *ip;
	try { ip = new (eth->data<void>()) Ipv4_packet(ip_size); }
	catch (Ipv4_packet::No_ip_packet) { log("Invalid IP packet"); return; }

	uint8_t      tlp      = ip->protocol();
	size_t       tlp_size = ip->total_length() - ip->header_length() * 4;
	void        *tlp_ptr  = tlp_packet(tlp, ip, tlp_size);
	uint16_t     dst_port = tlp_dst_port(tlp, tlp_ptr);
	Interface   *interface  = nullptr;
	Ipv4_address to       = ip->dst();
	Ipv4_address via      = ip->dst();

	/* ... first try to find a matching proxy route ... */
	interface = _tlp_proxy_route(tlp, tlp_ptr, dst_port, ip, to, via);

	/* ... if that fails go through all matching IP routes ... */
	if (!interface) {

		Ip_route * route = _ip_routes.first();
		for (; route; route = route->next()) {

			/* ... try all port routes of the current IP route ... */
			if (!route->matches(ip->dst())) {
				continue; }

			Port_route *port = tlp_port_list(tlp, route)->first();
			for (; port; port = port->next()) {

				if (port->dst() != dst_port) {
					continue; }

				interface = _interface_tree.find_by_label(port->label().string());
				if (interface) {

					bool const to_set = port->to() != Ipv4_address();
					bool const via_set = port->via() != Ipv4_address();
					if (to_set && !via_set) {
						to = port->to();
						via = port->to();
						break;
					}
					if (via_set) {
						via = port->via(); }

					if (to_set) {
						to = port->to(); }

					break;
				}
			}
			if (interface) {
				break; }

			/* ... then try the IP route itself ... */
			interface = _interface_tree.find_by_label(route->label().string());
			if (interface) {

				bool const to_set = route->to() != Ipv4_address();
				bool const via_set = route->via() != Ipv4_address();
				if (to_set && !via_set) {
					to = route->to();
					via = route->to();
					break;
				}
				if (via_set) {
					via = route->via(); }

				if (to_set) {
					to = route->to(); }

				break;
			}
		}
	}

	/* ... and give up if no IP and port route matches */
	if (!interface) {
		if (_verbose) { log("Unroutable packet"); }
		return;
	}

	/* send ARP request if there is no ARP entry for the destination IP */
	try {
		Arp_cache_entry &arp_entry = _arp_cache.find_by_ip_addr(via);
		eth->dst(arp_entry.mac_addr().addr);

	} catch (Arp_cache::No_matching_entry) {

		interface->arp_broadcast(via);
		_arp_waiters.insert(new (_allocator) Arp_waiter(*this, via, *eth,
		                                                eth_size, *packet));

		ack_packet = false;
		return;
	}
	/* adapt packet to the collected info */
	eth->src(interface->router_mac());
	ip->dst(to);
	tlp_dst_port(tlp, tlp_ptr, dst_port);

	/* if configured, use proxy source IP */
	if (_proxy) {
		Ipv4_address client_ip = ip->src();
		ip->src(interface->router_ip());

		/* if also the source port doesn't match port routes, use proxy port */
		uint16_t src_port = tlp_src_port(tlp, tlp_ptr);
		Ip_route *dst_route = interface->ip_routes().first();
		for (; dst_route; dst_route = dst_route->next()) {
			if (tlp_port_tree(tlp, dst_route)->find_by_dst(src_port)) {
				break; }
		}
		if (!dst_route) {
			_tlp_apply_port_proxy(tlp, tlp_ptr, ip, client_ip, src_port); }
	}
	/* update checksums and deliver packet */
	tlp_update_checksum(tlp, tlp_ptr, ip->src(), ip->dst(), tlp_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));
	interface->send(eth, eth_size);
}


Tcp_proxy *Interface::_find_tcp_proxy_by_client(Ipv4_address ip, uint16_t port)
{
	Tcp_proxy *proxy = _tcp_proxies.first();
	while (proxy) {

		if (_chk_delete_tcp_proxy(proxy)) {
			continue; }

		if (proxy->matches_client(ip, port)) {
			break; }

		proxy = proxy->next();
	}
	return proxy;
}


Tcp_proxy *Interface::_find_tcp_proxy_by_proxy(Ipv4_address ip, uint16_t port)
{
	Tcp_proxy *proxy = _tcp_proxies.first();
	while (proxy) {

		if (_chk_delete_tcp_proxy(proxy)) {
			continue; }

		if (proxy->matches_proxy(ip, port)) {
			break; }

		proxy = proxy->next();
	}
	return proxy;
}


Udp_proxy *Interface::_find_udp_proxy_by_client(Ipv4_address ip, uint16_t port)
{
	Udp_proxy *proxy = _udp_proxies.first();
	while (proxy) {

		if (_chk_delete_udp_proxy(proxy)) {
			continue; }

		if (proxy->matches_client(ip, port)) {
			break; }

		proxy = proxy->next();
	}
	return proxy;
}


Udp_proxy *Interface::_find_udp_proxy_by_proxy(Ipv4_address ip, uint16_t port)
{
	Udp_proxy *proxy = _udp_proxies.first();
	while (proxy) {

		if (_chk_delete_udp_proxy(proxy)) {
			continue; }

		if (proxy->matches_proxy(ip, port)) {
			break; }

		proxy = proxy->next();
	}
	return proxy;
}


void Interface::arp_broadcast(Ipv4_address ip_addr)
{
	using Ethernet_arp = Ethernet_frame_sized<sizeof(Arp_packet)>;
	Ethernet_arp eth_arp(Mac_address(0xff), _router_mac, Ethernet_frame::ARP);

	void * const eth_data = eth_arp.data<void>();
	size_t const arp_size = sizeof(eth_arp) - sizeof(Ethernet_frame);
	Arp_packet * const arp = new (eth_data) Arp_packet(arp_size);

	arp->hardware_address_type(Arp_packet::ETHERNET);
	arp->protocol_address_type(Arp_packet::IPV4);
	arp->hardware_address_size(sizeof(Mac_address));
	arp->protocol_address_size(sizeof(Ipv4_address));
	arp->opcode(Arp_packet::REQUEST);
	arp->src_mac(_router_mac);
	arp->src_ip(_router_ip);
	arp->dst_mac(Mac_address(0xff));
	arp->dst_ip(ip_addr);

	send(&eth_arp, sizeof(eth_arp));
}


void Interface::_remove_arp_waiter(Arp_waiter *arp_waiter)
{
	_arp_waiters.remove(arp_waiter);
	destroy(arp_waiter->interface().allocator(), arp_waiter);
}


Arp_waiter *Interface::_new_arp_entry(Arp_waiter *arp_waiter,
                                      Arp_cache_entry *arp_entry)
{
	Arp_waiter *next_arp_waiter = arp_waiter->next();
	if (arp_waiter->new_arp_cache_entry(*arp_entry)) {
		_remove_arp_waiter(arp_waiter); }

	return next_arp_waiter;
}


void Interface::_handle_arp_reply(Arp_packet * const arp)
{
	try {
		_arp_cache.find_by_ip_addr(arp->src_ip());
		if (_verbose) {
			log("ARP entry already exists"); }

		return;
	} catch (Arp_cache::No_matching_entry) {
		try {
			Arp_cache_entry *arp_entry =
				new (env()->heap()) Arp_cache_entry(arp->src_ip(),
				                                    arp->src_mac());

			_arp_cache.insert(arp_entry);
			Arp_waiter *arp_waiter = _arp_waiters.first();
			for (; arp_waiter; arp_waiter = _new_arp_entry(arp_waiter, arp_entry)) { }

		} catch (Allocator::Out_of_memory) {

			if (_verbose) {
				error("failed to allocate new ARP cache entry"); }

			return;
		}
	}
}


void Interface::_handle_arp_request(Ethernet_frame * const eth,
                                    size_t const eth_size,
                                    Arp_packet * const arp)
{
	/* ignore packets that do not target the router */
	if (arp->dst_ip() != router_ip()) {
		if (_verbose) {
			log("ARP does not target router"); }

		return;
	}
	/* interchange source and destination MAC and IP addresses */
	arp->dst_ip(arp->src_ip());
	arp->dst_mac(arp->src_mac());
	eth->dst(eth->src());
	arp->src_ip(router_ip());
	arp->src_mac(router_mac());
	eth->src(router_mac());

	/* mark packet as reply and send it back to its sender */
	arp->opcode(Arp_packet::REPLY);
	send(eth, eth_size);
}


void Interface::_handle_arp(Ethernet_frame *eth, size_t eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) {
		if (_verbose) {
			log("ARP for unknown protocol");
			return;
		}
	}
	switch (arp->opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp); break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp); break;
	default: if (_verbose) { log("unknown ARP operation"); } }
}


void Interface::_ready_to_submit(unsigned)
{
	while (sink()->packet_avail()) {

		_packet = sink()->get_packet();
		if (!_packet.size()) {
			continue; }

		if (_verbose) {
			Genode::printf("<< %s ", Interface::string());
			dump_eth(sink()->packet_content(_packet), _packet.size());
			Genode::printf("\n");
		}
		bool ack = true;
		handle_ethernet(sink()->packet_content(_packet), _packet.size(), ack,
		                &_packet);

		if (!ack) {
			continue; }

		if (!sink()->ready_to_ack()) {
			if (_verbose) {
				log("Ack state FULL"); }

			return;
		}
		sink()->acknowledge_packet(_packet);
	}
}


void Interface::continue_handle_ethernet(void *src, Genode::size_t size,
                                         Packet_descriptor *packet)
{
	bool ack = true;
	handle_ethernet(src, size, ack, packet);
	if (!ack) {
		if (_verbose) { log("Failed to continue eth handling"); }
		return;
	}
	if (!sink()->ready_to_ack()) {
		if (_verbose) { log("Ack state FULL"); }
		return;
	}
	sink()->acknowledge_packet(*packet);
}


void Interface::_ready_to_ack(unsigned)
{
	while (source()->ack_avail()) {
		source()->release_packet(source()->get_acked_packet()); }
}


void Interface::handle_ethernet(void *src, size_t size, bool &ack,
                                Packet_descriptor *packet)
{
	try {
		Ethernet_frame * const eth = new (src) Ethernet_frame(size);
		switch (eth->type()) {
		case Ethernet_frame::ARP:  _handle_arp(eth, size); break;
		case Ethernet_frame::IPV4: _handle_ip(eth, size, ack, packet); break;
		default: ; }
	}
	catch (Ethernet_frame::No_ethernet_frame) {
		error("Invalid ethernet frame at ", label()); }

	catch (Too_many_tcp_proxies) {
		error("Too many TCP NAT links requested by ", label()); }
}


void Interface::send(Ethernet_frame *eth, Genode::size_t size)
{
	if (_verbose) {
		Genode::printf(">> %s ", Interface::string());
		dump_eth(eth, size);
		Genode::printf("\n");
	}
	try {
		/* copy and submit packet */
		Packet_descriptor packet  = source()->alloc_packet(size);
		char             *content = source()->packet_content(packet);
		Genode::memcpy((void *)content, (void *)eth, size);
		source()->submit_packet(packet);

	} catch (Packet_stream_source< ::Nic::Session::Policy>::Packet_alloc_failed) {
		if (_verbose) {
			log("Failed to allocate packet"); }
	}
}


void Interface::_read_route(Xml_node &route_xn)
{
	Ipv4_address_prefix dst =
		route_xn.attribute_value("dst", Ipv4_address_prefix());

	Ipv4_address const via = route_xn.attribute_value("via", Ipv4_address());
	Ipv4_address const to = route_xn.attribute_value("to", Ipv4_address());
	Ip_route *route;
	try {
		char const *in = route_xn.attribute("label").value_base();
		size_t in_sz    = route_xn.attribute("label").value_size();
		route = new (_allocator) Ip_route(dst.address, dst.prefix, via, to, in,
		                                  in_sz, _allocator, route_xn,
		                                  _verbose);

	} catch (Xml_attribute::Nonexistent_attribute) {
		route = new (_allocator) Ip_route(dst.address, dst.prefix, via, to,
		                                  "", 0, _allocator, route_xn,
		                                  _verbose);
	}
	_ip_routes.insert(route);
	if (_verbose) {
		log("  IP route: ", *route); }
}


Interface::Interface(Server::Entrypoint    &ep,
                     Mac_address const      router_mac,
                     Ipv4_address const     router_ip,
                     Genode::Allocator     &allocator,
                     char const            *args,
                     Port_allocator        &tcp_port_alloc,
                     Port_allocator        &udp_port_alloc,
                     Mac_address const      mac,
                     Tcp_proxy_list        &tcp_proxies,
                     Udp_proxy_list        &udp_proxies,
                     unsigned const         rtt_sec,
                     Interface_tree        &interface_tree,
                     Arp_cache             &arp_cache,
                     Arp_waiter_list       &arp_waiters,
                     bool                   verbose)
:
	Session_label(label_from_args(args)),
	Avl_string_base(Session_label::string()),
	_sink_ack(ep, *this, &Interface::_ack_avail),
	_sink_submit(ep, *this, &Interface::_ready_to_submit),
	_source_ack(ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail), _ep(ep),
	_router_mac(router_mac), _router_ip(router_ip), _mac(mac),
	_allocator(allocator), _policy(*static_cast<Session_label *>(this)),
	_proxy(_policy.attribute_value("nat", false)), _tcp_proxies(tcp_proxies),
	_tcp_port_alloc(tcp_port_alloc), _udp_proxies(udp_proxies),
	_udp_port_alloc(udp_port_alloc), _rtt_sec(rtt_sec),
	_interface_tree(interface_tree), _arp_cache(arp_cache),
	_arp_waiters(arp_waiters), _verbose(verbose)
{
	if (_proxy) {
		_tcp_proxy      = _policy.attribute_value("nat-tcp-ports", 0UL);
		_tcp_proxy_used = 0;
		_udp_proxy      = _policy.attribute_value("nat-udp-ports", 0UL);
		_udp_proxy_used = 0;
	}
	if (_verbose) {
		log("Interface \"", *static_cast<Session_label *>(this), "\"");
		log("  MAC ", _mac);
		log("  Router identity: MAC ", _router_mac, " IP ", _router_ip);
		if (_proxy) {
			log("  NAT TCP ports: ", _tcp_proxy, " UDP ports: ", _udp_proxy);
		} else {
			log("  NAT off");
		}
	}
	try {
		Xml_node route = _policy.sub_node("ip");
		for (; ; route = route.next("ip")) { _read_route(route); }

	} catch (Xml_node::Nonexistent_sub_node) { }
	_interface_tree.insert(this);
}


Interface::~Interface()
{
	/* make interface unfindable */
	_interface_tree.remove(this);

	/* delete all ARP requests of this interface */
	Arp_waiter *arp_waiter = _arp_waiters.first();
	while (arp_waiter) {

		Arp_waiter *next_arp_waiter = arp_waiter->next();
		if (&arp_waiter->interface() != this) {
			_remove_arp_waiter(arp_waiter); }

		arp_waiter = next_arp_waiter;
	}
	/* delete all UDP proxies of this interface */
	Udp_proxy *udp_proxy = _udp_proxies.first();
	while (udp_proxy) {

		Udp_proxy *next_udp_proxy = udp_proxy->next();
		if (&udp_proxy->client() == this) {
			_delete_udp_proxy(udp_proxy); }

		udp_proxy = next_udp_proxy;
	}
	/* delete all TCP proxies of this interface */
	Tcp_proxy *tcp_proxy = _tcp_proxies.first();
	while (tcp_proxy) {

		Tcp_proxy *next_tcp_proxy = tcp_proxy->next();
		if (&tcp_proxy->client() == this) {
			_delete_tcp_proxy(tcp_proxy); }

		tcp_proxy = next_tcp_proxy;
	}
}


Interface *Interface_tree::find_by_label(char const *label)
{
	if (!strcmp(label, "")) {
		return nullptr; }

	Interface *interface = static_cast<Interface *>(first());
	if (!interface) {
		return nullptr; }

	interface = static_cast<Interface *>(interface->find_by_name(label));
	return interface;
}
