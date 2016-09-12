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
#include <net/tcp.h>
#include <net/udp.h>
#include <net/arp.h>

/* local includes */
#include <interface.h>
#include <configuration.h>
#include <protocol_name.h>

using namespace Net;
using namespace Genode;


/***************
 ** Utilities **
 ***************/

template <typename LINK_TYPE>
static void _destroy_closed_links(Link_list   &closed_links,
                                  Deallocator &dealloc)
{
	while (Link *link = closed_links.first()) {
		closed_links.remove(link);
		destroy(dealloc, static_cast<LINK_TYPE *>(link));
	}
}


template <typename LINK_TYPE>
static void _destroy_links(Link_side_tree &links,
                           Link_list      &closed_links,
                           Deallocator    &dealloc)
{
	_destroy_closed_links<LINK_TYPE>(closed_links, dealloc);
	while (Link_side *link_side = links.first()) {
		Link &link = link_side->link();
		link.dissolve();
		destroy(dealloc, static_cast<LINK_TYPE *>(&link));
	}
}


static void _link_packet(uint8_t  const  prot,
                         void    *const  prot_base,
                         Link           &link,
                         bool     const  client)
{
	switch (prot) {
	case Tcp_packet::IP_ID:
		if (client) {
			static_cast<Tcp_link *>(&link)->client_packet(*(Tcp_packet *)(prot_base));
			return;
		} else {
			static_cast<Tcp_link *>(&link)->server_packet(*(Tcp_packet *)(prot_base));
			return;
		}
	case Udp_packet::IP_ID:
		static_cast<Udp_link *>(&link)->packet();
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void _update_checksum(uint8_t       const prot,
                             void         *const prot_base,
                             size_t        const prot_size,
                             Ipv4_address  const src,
                             Ipv4_address  const dst)
{
	switch (prot) {
	case Tcp_packet::IP_ID:
		((Tcp_packet *)prot_base)->update_checksum(src, dst, prot_size);
		return;
	case Udp_packet::IP_ID:
		((Udp_packet *)prot_base)->update_checksum(src, dst);
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static uint16_t _dst_port(uint8_t  const prot,
                          void    *const prot_base)
{
	switch (prot) {
	case Tcp_packet::IP_ID: return (*(Tcp_packet *)prot_base).dst_port();
	case Udp_packet::IP_ID: return (*(Udp_packet *)prot_base).dst_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _dst_port(uint8_t   const prot,
                      void     *const prot_base,
                      uint16_t  const port)
{
	switch (prot) {
	case Tcp_packet::IP_ID: (*(Tcp_packet *)prot_base).dst_port(port); return;
	case Udp_packet::IP_ID: (*(Udp_packet *)prot_base).dst_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static uint16_t _src_port(uint8_t  const prot,
                          void    *const prot_base)
{
	switch (prot) {
	case Tcp_packet::IP_ID: return (*(Tcp_packet *)prot_base).src_port();
	case Udp_packet::IP_ID: return (*(Udp_packet *)prot_base).src_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _src_port(uint8_t   const prot,
                      void     *const prot_base,
                      uint16_t  const port)
{
	switch (prot) {
	case Tcp_packet::IP_ID: ((Tcp_packet *)prot_base)->src_port(port); return;
	case Udp_packet::IP_ID: ((Udp_packet *)prot_base)->src_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void *_prot_base(uint8_t const  prot,
                        size_t  const  prot_size,
                        Ipv4_packet   &ip)
{
	switch (prot) {
	case Tcp_packet::IP_ID: return new (ip.data<void>()) Tcp_packet(prot_size);
	case Udp_packet::IP_ID: return new (ip.data<void>()) Udp_packet(prot_size);
	default: throw Interface::Bad_transport_protocol(); }
}


/***************
 ** Interface **
 ***************/

void Interface::_pass_ip(Ethernet_frame &eth,
                         size_t   const  eth_size,
                         Ipv4_packet    &ip,
                         uint8_t  const  prot,
                         void    *const  prot_base,
                         size_t   const  prot_size)
{
	_update_checksum(prot, prot_base, prot_size, ip.src(), ip.dst());
	ip.checksum(Ipv4_packet::calculate_checksum(ip));
	_send(eth, eth_size);
}


Forward_rule_tree &Interface::_forward_rules(uint8_t const prot) const
{
	switch (prot) {
	case Tcp_packet::IP_ID: return _domain.tcp_forward_rules();
	case Udp_packet::IP_ID: return _domain.udp_forward_rules();
	default: throw Bad_transport_protocol(); }
}


Transport_rule_list &Interface::_transport_rules(uint8_t const prot) const
{
	switch (prot) {
	case Tcp_packet::IP_ID: return _domain.tcp_rules();
	case Udp_packet::IP_ID: return _domain.udp_rules();
	default: throw Bad_transport_protocol(); }
}


void
Interface::_new_link(uint8_t                       const  protocol,
                     Link_side_id                  const &local,
                     Pointer<Port_allocator_guard> const  remote_port_alloc,
                     Interface                           &remote_interface,
                     Link_side_id                  const &remote)
{
	switch (protocol) {
	case Tcp_packet::IP_ID:
		{
			Tcp_link &link = *new (_alloc)
				Tcp_link(*this, local, remote_port_alloc, remote_interface,
				         remote, _timer, _config(), protocol);
			_tcp_links.insert(&link.client());
			remote_interface._tcp_links.insert(&link.server());
			if (_config().verbose()) {
				log("New TCP client link: ", link.client(), " at ", *this);
				log("New TCP server link: ", link.server(),
				    " at ", remote_interface._domain);
			}
			return;
		}
	case Udp_packet::IP_ID:
		{
			Udp_link &link = *new (_alloc)
				Udp_link(*this, local, remote_port_alloc, remote_interface,
				         remote, _timer, _config(), protocol);
			_udp_links.insert(&link.client());
			remote_interface._udp_links.insert(&link.server());
			if (_config().verbose()) {
				log("New UDP client link: ", link.client(), " at ", *this);
				log("New UDP server link: ", link.server(),
				    " at ", remote_interface._domain);
			}
			return;
		}
	default: throw Bad_transport_protocol(); }
}


Link_side_tree &Interface::_links(uint8_t const protocol)
{
	switch (protocol) {
	case Tcp_packet::IP_ID: return _tcp_links;
	case Udp_packet::IP_ID: return _udp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::link_closed(Link &link, uint8_t const prot)
{
	_closed_links(prot).insert(&link);
}


void Interface::dissolve_link(Link_side &link_side, uint8_t const prot)
{
	_links(prot).remove(&link_side);
}


Link_list &Interface::_closed_links(uint8_t const protocol)
{
	switch (protocol) {
	case Tcp_packet::IP_ID: return _closed_tcp_links;
	case Udp_packet::IP_ID: return _closed_udp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::_adapt_eth(Ethernet_frame          &eth,
                           size_t            const  eth_size,
                           Ipv4_address      const &ip,
                           Packet_descriptor const &pkt,
                           Interface               &interface)
{
	Ipv4_address const &hop_ip = interface._domain.next_hop(ip);
	try { eth.dst(interface._arp_cache.find_by_ip(hop_ip).mac()); }
	catch (Arp_cache::No_match) {
		interface._broadcast_arp_request(hop_ip);
		new (_alloc) Arp_waiter(*this, interface, hop_ip, pkt);
		throw Packet_postponed();
	}
	eth.src(_router_mac);
}


void Interface::_nat_link_and_pass(Ethernet_frame      &eth,
                                   size_t        const  eth_size,
                                   Ipv4_packet         &ip,
                                   uint8_t       const  prot,
                                   void         *const  prot_base,
                                   size_t        const  prot_size,
                                   Link_side_id  const &local,
                                   Interface           &interface)
{
	Pointer<Port_allocator_guard> remote_port_alloc;
	try {
		Nat_rule &nat = interface._domain.nat_rules().find_by_domain(_domain);
		if(_config().verbose()) {
			log("Using NAT rule: ", nat); }

		_src_port(prot, prot_base, nat.port_alloc(prot).alloc());
		ip.src(interface._router_ip());
		remote_port_alloc.set(nat.port_alloc(prot));
	}
	catch (Nat_rule_tree::No_match) { }
	Link_side_id const remote = { ip.dst(), _dst_port(prot, prot_base),
	                              ip.src(), _src_port(prot, prot_base) };
	_new_link(prot, local, remote_port_alloc, interface, remote);
	interface._pass_ip(eth, eth_size, ip, prot, prot_base, prot_size);
}


void Interface::_handle_ip(Ethernet_frame          &eth,
                           Genode::size_t    const  eth_size,
                           Packet_descriptor const &pkt)
{
	_destroy_closed_links<Udp_link>(_closed_udp_links, _alloc);
	_destroy_closed_links<Tcp_link>(_closed_tcp_links, _alloc);

	/* read packet information */
	Ipv4_packet &ip = *new (eth.data<void>())
		Ipv4_packet(eth_size - sizeof(Ethernet_frame));

	uint8_t       const prot      = ip.protocol();
	size_t        const prot_size = ip.total_length() - ip.header_length() * 4;
	void         *const prot_base = _prot_base(prot, prot_size, ip);
	Link_side_id  const local     = { ip.src(), _src_port(prot, prot_base),
	                                  ip.dst(), _dst_port(prot, prot_base) };

	/* try to route via existing UDP/TCP links */
	try {
		Link_side const &local_side = _links(prot).find_by_id(local);
		Link &link = local_side.link();
		bool const client = local_side.is_client();
		Link_side &remote_side = client ? link.server() : link.client();
		Interface &interface = remote_side.interface();
		if(_config().verbose()) {
			log("Using ", protocol_name(prot), " link: ", link); }

		_adapt_eth(eth, eth_size, remote_side.src_ip(), pkt, interface);
		ip.src(remote_side.dst_ip());
		ip.dst(remote_side.src_ip());
		_src_port(prot, prot_base, remote_side.dst_port());
		_dst_port(prot, prot_base, remote_side.src_port());

		interface._pass_ip(eth, eth_size, ip, prot, prot_base, prot_size);
		_link_packet(prot, prot_base, link, client);
		return;
	}
	catch (Link_side_tree::No_match) { }

	/* try to route via forward rules */
	if (local.dst_ip == _router_ip()) {
		try {
			Forward_rule const &rule =
				_forward_rules(prot).find_by_port(local.dst_port);

			Interface &interface = rule.domain().interface().deref();
			if(_config().verbose()) {
				log("Using forward rule: ", protocol_name(prot), " ", rule); }

			_adapt_eth(eth, eth_size, rule.to(), pkt, interface);
			ip.dst(rule.to());
			_nat_link_and_pass(eth, eth_size, ip, prot, prot_base, prot_size,
			                   local, interface);
			return;
		}
		catch (Forward_rule_tree::No_match) { }
		catch (Pointer<Interface>::Invalid) { }
	}
	/* try to route via transport and permit rules */
	try {
		Transport_rule const &transport_rule =
			_transport_rules(prot).longest_prefix_match(local.dst_ip);

		Permit_rule const &permit_rule =
			transport_rule.permit_rule(local.dst_port);

		Interface &interface = permit_rule.domain().interface().deref();
		if(_config().verbose()) {
			log("Using ", protocol_name(prot), " rule: ", transport_rule,
			    " ", permit_rule); }

		_adapt_eth(eth, eth_size, local.dst_ip, pkt, interface);
		_nat_link_and_pass(eth, eth_size, ip, prot, prot_base, prot_size,
		                   local, interface);
		return;
	}
	catch (Transport_rule_list::No_match) { }
	catch (Permit_single_rule_tree::No_match) { }
	catch (Pointer<Interface>::Invalid) { }

	/* try to route via IP rules */
	try {
		Ip_rule const &rule =
			_domain.ip_rules().longest_prefix_match(local.dst_ip);

		Interface &interface = rule.domain().interface().deref();
		if(_config().verbose()) {
			log("Using IP rule: ", rule); }

		_adapt_eth(eth, eth_size, local.dst_ip, pkt, interface);
		interface._pass_ip(eth, eth_size, ip, prot, prot_base, prot_size);
		return;
	}
	catch (Ip_rule_list::No_match) { }
	catch (Pointer<Interface>::Invalid) { }

	/* give up and drop packet */
	if (_config().verbose()) {
		log("Unroutable packet"); }
}


void Interface::_broadcast_arp_request(Ipv4_address const &ip)
{
	using Ethernet_arp = Ethernet_frame_sized<sizeof(Arp_packet)>;
	Ethernet_arp eth_arp(Mac_address(0xff), _router_mac, Ethernet_frame::ARP);
	void *const eth_data = eth_arp.data<void>();
	size_t const arp_size = sizeof(eth_arp) - sizeof(Ethernet_frame);
	Arp_packet &arp = *new (eth_data) Arp_packet(arp_size);
	arp.hardware_address_type(Arp_packet::ETHERNET);
	arp.protocol_address_type(Arp_packet::IPV4);
	arp.hardware_address_size(sizeof(Mac_address));
	arp.protocol_address_size(sizeof(Ipv4_address));
	arp.opcode(Arp_packet::REQUEST);
	arp.src_mac(_router_mac);
	arp.src_ip(_router_ip());
	arp.dst_mac(Mac_address(0xff));
	arp.dst_ip(ip);
	_send(eth_arp, sizeof(eth_arp));
}


void Interface::_handle_arp_reply(Arp_packet &arp)
{
	/* do nothing if ARP info already exists */
	try {
		_arp_cache.find_by_ip(arp.src_ip());
		if (_config().verbose()) {
			log("ARP entry already exists"); }

		return;
	}
	/* create cache entry and continue handling of matching packets */
	catch (Arp_cache::No_match) {
		Ipv4_address const ip = arp.src_ip();
		_arp_cache.new_entry(ip, arp.src_mac());
		for (Arp_waiter_list_element *waiter_le = _foreign_arp_waiters.first();
		     waiter_le; )
		{
			Arp_waiter &waiter = *waiter_le->object();
			waiter_le = waiter_le->next();
			if (ip != waiter.ip()) { continue; }
			waiter.src()._continue_handle_eth(waiter.packet());
			destroy(waiter.src()._alloc, &waiter);
		}
	}
}


Ipv4_address const &Interface::_router_ip() const
{
	return _domain.interface_attr().address;
}


void Interface::_handle_arp_request(Ethernet_frame &eth,
                                    size_t const    eth_size,
                                    Arp_packet     &arp)
{
	/* ignore packets that do not target the router */
	if (arp.dst_ip() != _router_ip()) {
		if (_config().verbose()) {
			log("ARP request for unknown IP"); }

		return;
	}
	/* interchange source and destination MAC and IP addresses */
	arp.dst_ip(arp.src_ip());
	arp.dst_mac(arp.src_mac());
	eth.dst(eth.src());
	arp.src_ip(_router_ip());
	arp.src_mac(_router_mac);
	eth.src(_router_mac);

	/* mark packet as reply and send it back to its sender */
	arp.opcode(Arp_packet::REPLY);
	_send(eth, eth_size);
}


void Interface::_handle_arp(Ethernet_frame &eth, size_t const eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t const arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet &arp = *new (eth.data<void>()) Arp_packet(arp_size);
	if (!arp.ethernet_ipv4()) {
		error("ARP for unknown protocol"); }

	switch (arp.opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp); break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp); break;
	default: error("unknown ARP operation"); }
}


void Interface::_ready_to_submit()
{
	while (_sink().packet_avail()) {

		Packet_descriptor const pkt = _sink().get_packet();
		if (!pkt.size()) {
			continue; }

		try { _handle_eth(_sink().packet_content(pkt), pkt.size(), pkt); }
		catch (Packet_postponed) { continue; }
		_ack_packet(pkt);
	}
}


void Interface::_continue_handle_eth(Packet_descriptor const &pkt)
{
	try { _handle_eth(_sink().packet_content(pkt), pkt.size(), pkt); }
	catch (Packet_postponed) { error("failed twice to handle packet"); }
	_ack_packet(pkt);
}


void Interface::_ready_to_ack()
{
	while (_source().ack_avail()) {
		_source().release_packet(_source().get_acked_packet()); }
}


void Interface::_handle_eth(void              *const  eth_base,
                            size_t             const  eth_size,
                            Packet_descriptor  const &pkt)
{
	try {
		Ethernet_frame * const eth = new (eth_base) Ethernet_frame(eth_size);
		if (_config().verbose()) {
			log("at ", _domain, " handle ", *eth); }

		switch (eth->type()) {
		case Ethernet_frame::ARP:  _handle_arp(*eth, eth_size);     break;
		case Ethernet_frame::IPV4: _handle_ip(*eth, eth_size, pkt); break;
		default: throw Bad_network_protocol(); }
	}
	catch (Ethernet_frame::No_ethernet_frame) {
		error("invalid ethernet frame"); }

	catch (Interface::Bad_transport_protocol) {
		error("unknown transport layer protocol"); }

	catch (Interface::Bad_network_protocol) {
		error("unknown network layer protocol"); }

	catch (Ipv4_packet::No_ip_packet) {
		error("invalid IP packet"); }

	catch (Port_allocator_guard::Out_of_indices) {
		error("no available NAT ports"); }

	catch (Domain::No_next_hop) {
		error("can not find next hop"); }
}


void Interface::_send(Ethernet_frame &eth, Genode::size_t const size)
{
	if (_config().verbose()) {
		log("at ", _domain, " send ", eth); }
	try {
		/* copy and submit packet */
		Packet_descriptor const pkt = _source().alloc_packet(size);
		char *content = _source().packet_content(pkt);
		Genode::memcpy((void *)content, (void *)&eth, size);
		_source().submit_packet(pkt);
	}
	catch (Packet_stream_source::Packet_alloc_failed) {
		if (_config().verbose()) {
			log("Failed to allocate packet"); }
	}
}


Interface::Interface(Entrypoint        &ep,
                     Genode::Timer     &timer,
                     Mac_address const  router_mac,
                     Genode::Allocator &alloc,
                     Mac_address const  mac,
                     Domain            &domain)
:
	_sink_ack(ep, *this, &Interface::_ack_avail),
	_sink_submit(ep, *this, &Interface::_ready_to_submit),
	_source_ack(ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail),
	_router_mac(router_mac), _mac(mac), _timer(timer), _alloc(alloc),
	_domain(domain)
{
	if (_config().verbose()) {
		log("Interface connected ", *this);
		log("  MAC ", _mac);
		log("  Router identity: MAC ", _router_mac, " IP ",
		    _router_ip(), "/", _domain.interface_attr().prefix);
	}
	_domain.interface().set(*this);
}


void Interface::_ack_packet(Packet_descriptor const &pkt)
{
	if (!_sink().ready_to_ack()) {
		error("ack state FULL");
		return;
	}
	_sink().acknowledge_packet(pkt);
}


void Interface::_cancel_arp_waiting(Arp_waiter &waiter)
{
	warning("waiting for ARP cancelled");
	_ack_packet(waiter.packet());
	destroy(_alloc, &waiter);
}


Interface::~Interface()
{
	_domain.interface().unset();
	if (_config().verbose()) {
		log("Interface disconnected ", *this); }

	/* destroy ARP waiters */
	while (_own_arp_waiters.first()) {
		_cancel_arp_waiting(*_foreign_arp_waiters.first()->object()); }

	while (_foreign_arp_waiters.first()) {
		Arp_waiter &waiter = *_foreign_arp_waiters.first()->object();
		waiter.src()._cancel_arp_waiting(waiter); }

	/* destroy links */
	_destroy_links<Tcp_link>(_tcp_links, _closed_tcp_links, _alloc);
	_destroy_links<Udp_link>(_udp_links, _closed_udp_links, _alloc);
}


Configuration &Interface::_config() const { return _domain.config(); }


void Interface::print(Output &output) const
{
	Genode::print(output, "\"", _domain.name(), "\"");
}
