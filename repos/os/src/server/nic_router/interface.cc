/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/tcp.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <net/arp.h>
#include <net/internet_checksum.h>
#include <base/quota_guard.h>

/* local includes */
#include <interface.h>
#include <configuration.h>
#include <l3_protocol.h>
#include <assertion.h>
#include <retry.h>

using namespace Net;
using Genode::Deallocator;
using Genode::size_t;
using Genode::uint32_t;
using Genode::addr_t;
using Genode::log;
using Genode::error;
using Genode::Exception;
using Genode::Out_of_ram;
using Genode::Out_of_caps;
using Genode::Constructible;
using Genode::Reconstructible;
using Genode::Signal_context_capability;
using Genode::Signal_transmitter;
using Dhcp_options = Dhcp_packet::Options_aggregator<Size_guard>;


/***************
 ** Utilities **
 ***************/

enum {

	ETHERNET_HEADER_SIZE = sizeof(Ethernet_frame),

	ETHERNET_DATA_SIZE_WITH_ARP =
		sizeof(Arp_packet) + ETHERNET_HEADER_SIZE < Ethernet_frame::MIN_SIZE ?
			Ethernet_frame::MIN_SIZE - ETHERNET_HEADER_SIZE :
			sizeof(Arp_packet),

	ETHERNET_CRC_SIZE = sizeof(Genode::uint32_t),

	ARP_PACKET_SIZE =
		ETHERNET_HEADER_SIZE +
		ETHERNET_DATA_SIZE_WITH_ARP +
		ETHERNET_CRC_SIZE,
};


template <typename LINK_TYPE>
static void _destroy_dissolved_links(Link_list   &dissolved_links,
                                     Deallocator &dealloc)
{
	while (Link *link = dissolved_links.first()) {
		dissolved_links.remove(link);
		destroy(dealloc, static_cast<LINK_TYPE *>(link));
	}
}


template <typename LINK_TYPE>
static void _destroy_link(Link        &link,
                          Link_list   &links,
                          Deallocator &dealloc)
{
	link.dissolve(false);
	links.remove(&link);
	destroy(dealloc, static_cast<LINK_TYPE *>(&link));
}


template <typename LINK_TYPE>
static void _destroy_links(Link_list   &links,
                           Link_list   &dissolved_links,
                           Deallocator &dealloc)
{
	_destroy_dissolved_links<LINK_TYPE>(dissolved_links, dealloc);
	while (Link *link = links.first()) {
		_destroy_link<LINK_TYPE>(*link, links, dealloc); }
}


template <typename LINK_TYPE>
static void _early_drop_links(Link_list     &links,
                              Link_list     &dissolved_links,
                              Deallocator   &dealloc,
                              unsigned long &max)
{
	if (!max) {
		return; }

	while (Link *link = dissolved_links.first()) {
		dissolved_links.remove(link);
		destroy(dealloc, static_cast<LINK_TYPE *>(link));
		if (!--max) {
			return; }
	}
	Link *link_ptr = links.first();
	while (link_ptr) {
		Link *next_ptr = link_ptr->next();
		if (static_cast<LINK_TYPE *>(link_ptr)->can_early_drop()) {
			_destroy_link<LINK_TYPE>(*link_ptr, links, dealloc);
			if (!--max) {
				return; }
		}
		link_ptr = next_ptr;
	}
}


static void _link_packet(L3_protocol  const  prot,
                         void        *const  prot_base,
                         Link               &link,
                         bool         const  client)
{
	switch (prot) {
	case L3_protocol::TCP:
		if (client) {
			static_cast<Tcp_link *>(&link)->client_packet(*(Tcp_packet *)(prot_base));
			return;
		} else {
			static_cast<Tcp_link *>(&link)->server_packet(*(Tcp_packet *)(prot_base));
			return;
		}
	case L3_protocol::UDP:
		if (client) {
			static_cast<Udp_link *>(&link)->client_packet();
			return;
		} else {
			static_cast<Udp_link *>(&link)->server_packet();
			return;
		}
		return;
	case L3_protocol::ICMP:
		if (client) {
			static_cast<Icmp_link *>(&link)->client_packet();
			return;
		} else {
			static_cast<Icmp_link *>(&link)->server_packet();
			return;
		}
		return;
	default: ASSERT_NEVER_REACHED; }
}


static void _update_checksum(L3_protocol   const prot,
                             void         *const prot_base,
                             size_t        const prot_size,
                             Ipv4_address  const src,
                             Ipv4_address  const dst,
                             size_t        const ip_size)
{
	switch (prot) {
	case L3_protocol::TCP:
		((Tcp_packet *)prot_base)->update_checksum(src, dst, prot_size);
		return;
	case L3_protocol::UDP:
		((Udp_packet *)prot_base)->update_checksum(src, dst);
		return;
	case L3_protocol::ICMP:
		{
			Icmp_packet &icmp = *(Icmp_packet *)prot_base;
			icmp.update_checksum(ip_size - sizeof(Ipv4_packet) -
			                               sizeof(Icmp_packet));
			return;
		}
	default: ASSERT_NEVER_REACHED; }
}


static Port _dst_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP:  return (*(Tcp_packet *)prot_base).dst_port();
	case L3_protocol::UDP:  return (*(Udp_packet *)prot_base).dst_port();
	case L3_protocol::ICMP: return Port((*(Icmp_packet *)prot_base).query_id());
	default: ASSERT_NEVER_REACHED; }
}


static void _dst_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP:  (*(Tcp_packet *)prot_base).dst_port(port);  return;
	case L3_protocol::UDP:  (*(Udp_packet *)prot_base).dst_port(port);  return;
	case L3_protocol::ICMP: (*(Icmp_packet *)prot_base).query_id(port.value); return;
	default: ASSERT_NEVER_REACHED; }
}


static Port _src_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP:  return (*(Tcp_packet *)prot_base).src_port();
	case L3_protocol::UDP:  return (*(Udp_packet *)prot_base).src_port();
	case L3_protocol::ICMP: return Port((*(Icmp_packet *)prot_base).query_id());
	default: ASSERT_NEVER_REACHED; }
}


static void _src_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP:  ((Tcp_packet *)prot_base)->src_port(port);        return;
	case L3_protocol::UDP:  ((Udp_packet *)prot_base)->src_port(port);        return;
	case L3_protocol::ICMP: ((Icmp_packet *)prot_base)->query_id(port.value); return;
	default: ASSERT_NEVER_REACHED; }
}


static void *_prot_base(L3_protocol const  prot,
                        Size_guard        &size_guard,
                        Ipv4_packet       &ip)
{
	switch (prot) {
	case L3_protocol::TCP:  return &ip.data<Tcp_packet>(size_guard);
	case L3_protocol::UDP:  return &ip.data<Udp_packet>(size_guard);
	case L3_protocol::ICMP: return &ip.data<Icmp_packet>(size_guard);
	default: ASSERT_NEVER_REACHED; }
}


static bool _supported_transport_prot(L3_protocol prot)
{
	return
		prot == L3_protocol::TCP ||
		prot == L3_protocol::UDP ||
		prot == L3_protocol::ICMP;
}


static void _with_forward_rule(Domain &dom, L3_protocol prot, Port port, auto const &fn)
{
	switch (prot) {
	case L3_protocol::TCP: dom.tcp_forward_rules().find_by_port(port, [&] (Forward_rule const &r) { fn(r); }, []{}); break;
	case L3_protocol::UDP: dom.udp_forward_rules().find_by_port(port, [&] (Forward_rule const &r) { fn(r); }, []{}); break;
	default: break; }
}


static void _with_transport_rule(Domain &dom, L3_protocol prot, Ipv4_address const &ip, Port port, auto const &fn)
{
	switch (prot) {
	case L3_protocol::TCP: dom.tcp_rules().find_best_match(ip, port, [&] (Transport_rule const &tr, Permit_rule const &pr) { fn(tr, pr); }, []{}); break;
	case L3_protocol::UDP: dom.udp_rules().find_best_match(ip, port, [&] (Transport_rule const &tr, Permit_rule const &pr) { fn(tr, pr); }, []{}); break;
	default: break; }
}

/**************************
 ** Interface_link_stats **
 **************************/

bool Interface_link_stats::report_empty() const
{
	return
		!refused_for_ram && !refused_for_ports && !opening && !open && !closing && !closed && !dissolved_timeout_opening &&
		!dissolved_timeout_open && !dissolved_timeout_closing && !dissolved_timeout_closed && !dissolved_no_timeout && !destroyed;
}


void Interface_link_stats::report(Genode::Xml_generator &xml) const
{
	if (refused_for_ram)           xml.node("refused_for_ram",           [&] { xml.attribute("value", refused_for_ram); });
	if (refused_for_ports)         xml.node("refused_for_ports",         [&] { xml.attribute("value", refused_for_ports); });
	if (opening)                   xml.node("opening",                   [&] { xml.attribute("value", opening); });
	if (open)                      xml.node("open",                      [&] { xml.attribute("value", open);    });
	if (closing)                   xml.node("closing",                   [&] { xml.attribute("value", closing); });
	if (closed)                    xml.node("closed",                    [&] { xml.attribute("value", closed);  });
	if (dissolved_timeout_opening) xml.node("dissolved_timeout_opening", [&] { xml.attribute("value", dissolved_timeout_opening); });
	if (dissolved_timeout_open)    xml.node("dissolved_timeout_open",    [&] { xml.attribute("value", dissolved_timeout_open);    });
	if (dissolved_timeout_closing) xml.node("dissolved_timeout_closing", [&] { xml.attribute("value", dissolved_timeout_closing); });
	if (dissolved_timeout_closed)  xml.node("dissolved_timeout_closed",  [&] { xml.attribute("value", dissolved_timeout_closed);  });
	if (dissolved_no_timeout)      xml.node("dissolved_no_timeout",      [&] { xml.attribute("value", dissolved_no_timeout);      });
	if (destroyed)                 xml.node("destroyed",                 [&] { xml.attribute("value", destroyed); });
}


/****************************
 ** Interface_object_stats **
 ****************************/

bool Interface_object_stats::report_empty() const { return !alive && !destroyed; }


void Interface_object_stats::report(Genode::Xml_generator &xml) const
{
	if (alive)     xml.node("alive",     [&] { xml.attribute("value", alive);     });
	if (destroyed) xml.node("destroyed", [&] { xml.attribute("value", destroyed); });
}


/***************
 ** Interface **
 ***************/

void Interface::destroy_link(Link &link)
{
	L3_protocol const prot = link.protocol();
	switch (prot) {
	case L3_protocol::TCP:  ::_destroy_link<Tcp_link>(link, links(prot), _alloc);  break;
	case L3_protocol::UDP:  ::_destroy_link<Udp_link>(link, links(prot), _alloc);  break;
	case L3_protocol::ICMP: ::_destroy_link<Icmp_link>(link, links(prot), _alloc); break;
	default: ASSERT_NEVER_REACHED; }
}


void Interface::_pass_prot_to_domain(Domain                       &domain,
                                     Ethernet_frame               &eth,
                                     Size_guard                   &size_guard,
                                     Ipv4_packet                  &ip,
                                     Internet_checksum_diff const &ip_icd,
                                     L3_protocol            const  prot,
                                     void                  *const  prot_base,
                                     size_t                 const  prot_size)
{
	_update_checksum(
		prot, prot_base, prot_size, ip.src(), ip.dst(), ip.total_length());

	ip.update_checksum(ip_icd);
	domain.interfaces().for_each([&] (Interface &interface)
	{
		eth.src(interface._router_mac);
		if (!domain.use_arp()) {
			eth.dst(interface._router_mac);
		}
		interface.send(eth, size_guard);
	});
}


void Interface::_attach_to_domain_raw(Domain &domain)
{
	_domain_ptr = &domain;
	_refetch_domain_ready_state();
	_interfaces.remove(this);
	domain.attach_interface(*this);
}


void Interface::_detach_from_domain_raw()
{
	Domain &domain = *_domain_ptr;
	domain.detach_interface(*this);
	_interfaces.insert(this);
	_domain_ptr = nullptr;
	_refetch_domain_ready_state();

	domain.add_dropped_fragm_ipv4(_dropped_fragm_ipv4);
	domain.tcp_stats().dissolve_interface(_tcp_stats);
	domain.udp_stats().dissolve_interface(_udp_stats);
	domain.icmp_stats().dissolve_interface(_icmp_stats);
	domain.arp_stats().dissolve_interface(_arp_stats);
	domain.dhcp_stats().dissolve_interface(_dhcp_stats);
}


void Interface::_update_domain_object(Domain &new_domain) {

	/* detach raw */
	Domain &old_domain = *_domain_ptr;
	old_domain.interface_updates_domain_object(*this);
	_interfaces.insert(this);
	_domain_ptr = nullptr;
	_refetch_domain_ready_state();

	old_domain.add_dropped_fragm_ipv4(_dropped_fragm_ipv4);
	old_domain.tcp_stats().dissolve_interface(_tcp_stats);
	old_domain.udp_stats().dissolve_interface(_udp_stats);
	old_domain.icmp_stats().dissolve_interface(_icmp_stats);
	old_domain.arp_stats().dissolve_interface(_arp_stats);
	old_domain.dhcp_stats().dissolve_interface(_dhcp_stats);

	/* attach raw */
	_domain_ptr = &new_domain;
	_refetch_domain_ready_state();
	_interfaces.remove(this);
	new_domain.attach_interface(*this);
}


void Interface::attach_to_domain()
{
	_config_ptr->domains().with_element(
		_policy.determine_domain_name(),
		[&] /* match_fn */ (Domain &domain)
		{
			_attach_to_domain_raw(domain);

			/* construct DHCP client if the new domain needs it */
			if (domain.ip_config_dynamic()) {
				_dhcp_client.construct(_timer, *this);
			}
			attach_to_domain_finish();
		},
		[&] /* no_match_fn */ () { }
	);
}


void Interface::attach_to_domain_finish()
{
	if (!link_state()) {
		return; }

	/* if domain has yet no IP config, participate in requesting one */
	Domain &domain = *_domain_ptr;
	Ipv4_config const &ip_config = domain.ip_config();
	if (!ip_config.valid()) {
		_dhcp_client->discover();
		return;
	}
	attach_to_ip_config(domain, ip_config);
}


void Interface::attach_to_ip_config(Domain            &domain,
                                    Ipv4_config const &ip_config)
{
	/* if others wait for ARP at the domain, participate in requesting it */
	domain.foreign_arp_waiters().for_each([&] (Arp_waiter_list_element &le) {
		_broadcast_arp_request(ip_config.interface().address,
		                       le.object()->ip());
	});
}


void Interface::detach_from_ip_config(Domain &domain)
{
	/* destroy our own ARP waiters */
	while (_own_arp_waiters.first()) {
		cancel_arp_waiting(*_own_arp_waiters.first()->object());
	}
	/* destroy links */
	_destroy_links<Tcp_link> (_tcp_links,  _dissolved_tcp_links,  _alloc);
	_destroy_links<Udp_link> (_udp_links,  _dissolved_udp_links,  _alloc);
	_destroy_links<Icmp_link>(_icmp_links, _dissolved_icmp_links, _alloc);

	/* destroy DHCP allocations */
	_destroy_released_dhcp_allocations(domain);
	while (Dhcp_allocation *allocation = _dhcp_allocations.first()) {
		_dhcp_allocations.remove(*allocation);
		_destroy_dhcp_allocation(*allocation, domain);
	}
	/* dissolve ARP cache entries with the MAC address of this interface */
	domain.arp_cache().destroy_entries_with_mac(_mac);
}


void Interface::handle_domain_ready_state(bool state)
{
	_policy.handle_domain_ready_state(state);
}


void Interface::_refetch_domain_ready_state()
{
	if (_domain_ptr)
		handle_domain_ready_state(_domain_ptr->ready());
	else
		handle_domain_ready_state(false);
}


void Interface::_reset_and_refetch_domain_ready_state()
{
	_policy.handle_domain_ready_state(false);
	_refetch_domain_ready_state();
}


void Interface::_detach_from_domain()
{
	with_domain([&] (Domain &domain) {
		detach_from_ip_config(domain);
		_detach_from_domain_raw(); });
}


void Interface::_try_emergency_free_quota()
{
	unsigned long max = MAX_FREE_OPS_PER_EMERGENCY;
	_early_drop_links<Icmp_link>(_icmp_links, _dissolved_icmp_links, _alloc, max);
	_early_drop_links<Udp_link> (_udp_links,  _dissolved_udp_links,  _alloc, max);
	_early_drop_links<Tcp_link> (_tcp_links,  _dissolved_tcp_links,  _alloc, max);
}


Packet_result Interface::_new_link(L3_protocol          const  protocol,
                                   void                *const  prot_base,
                                   Domain                     &local_domain,
                                   Link_side_id         const &local,
                                   Port_allocator_guard       *remote_port_alloc_ptr,
                                   Domain                     &remote_domain,
                                   Link_side_id         const &remote)
{
	Packet_result result { };
	switch (protocol) {
	case L3_protocol::TCP:
		retry_once<Out_of_ram, Out_of_caps>(
			[&] {
				new (_alloc)
					Tcp_link { *this, local_domain, local, remote_port_alloc_ptr, remote_domain,
					           remote, _timer, *_config_ptr, protocol, _tcp_stats, *(Tcp_packet *)prot_base }; },
			[&] { _try_emergency_free_quota(); },
			[&] {
				_tcp_stats.refused_for_ram++;
				result = packet_drop("out of quota while creating TCP link"); });
		break;
	case L3_protocol::UDP:
		retry_once<Out_of_ram, Out_of_caps>(
			[&] {
				new (_alloc)
					Udp_link { *this, local_domain, local, remote_port_alloc_ptr, remote_domain,
					           remote, _timer, *_config_ptr, protocol, _udp_stats }; },
			[&] { _try_emergency_free_quota(); },
			[&] {
				_udp_stats.refused_for_ram++;
				result = packet_drop("out of quota while creating UDP link"); });
		break;
	case L3_protocol::ICMP:
		retry_once<Out_of_ram, Out_of_caps>(
			[&] {
				new (_alloc)
					Icmp_link { *this, local_domain, local, remote_port_alloc_ptr, remote_domain,
					            remote, _timer, *_config_ptr, protocol, _icmp_stats }; },
			[&] { _try_emergency_free_quota(); },
			[&] {
				_icmp_stats.refused_for_ram++;
				result = packet_drop("out of quota while creating ICMP link"); });
		break;
	default: ASSERT_NEVER_REACHED; }
	return result;
}


void Interface::dhcp_allocation_expired(Dhcp_allocation &allocation)
{
	_release_dhcp_allocation(allocation, *_domain_ptr);
	_released_dhcp_allocations.insert(&allocation);
}


Link_list &Interface::links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _tcp_links;
	case L3_protocol::UDP:  return _udp_links;
	case L3_protocol::ICMP: return _icmp_links;
	default: ASSERT_NEVER_REACHED; }
}


Link_list &Interface::dissolved_links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _dissolved_tcp_links;
	case L3_protocol::UDP:  return _dissolved_udp_links;
	case L3_protocol::ICMP: return _dissolved_icmp_links;
	default: ASSERT_NEVER_REACHED; }
}


Packet_result Interface::_adapt_eth(Ethernet_frame          &eth,
                                    Ipv4_address      const &dst_ip,
                                    Packet_descriptor const &pkt,
                                    Domain                  &remote_domain)
{
	Packet_result result { };
	Ipv4_config const &remote_ip_cfg = remote_domain.ip_config();
	if (!remote_ip_cfg.valid()) {
		result = packet_drop("target domain has yet no IP config");
	}
	if (!remote_domain.use_arp())
		return result;

	auto with_next_hop = [&] (Ipv4_address const &hop_ip)
	{
		auto send_arp_fn = [&]
		{
			Packet_list_element &packet_le = *new (_alloc) Packet_list_element(pkt);
			auto create_new_arp_waiter = [&]
			{
				new (_alloc) Arp_waiter(
					*this, remote_domain, hop_ip, packet_le, _config_ptr->arp_request_timeout(), _timer);

				remote_domain.interfaces().for_each([&] (Interface &interface) {
					interface._broadcast_arp_request(remote_ip_cfg.interface().address, hop_ip); });

				result = packet_postponed();
			};
			remote_domain.foreign_arp_waiters().find_by_ip(hop_ip,
				[&] (Arp_waiter &waiter) { waiter.add_packet(packet_le); },
				[&] {
					retry_once<Out_of_ram, Out_of_caps>(
						create_new_arp_waiter,
						[&] { _try_emergency_free_quota(); },
						[&] { result = packet_drop("out of quota while creating ARP waiter"); });
				});
		};
		remote_domain.arp_cache().find_by_ip(
			hop_ip,
			[&] /* handle_match */ (Arp_cache_entry const &entry)
			{
				eth.dst(entry.mac());
			},
			[&] /* handle_no_match */ ()
			{
				retry_once<Out_of_ram, Out_of_caps>(
					send_arp_fn,
					[&] { _try_emergency_free_quota(); },
					[&] { result = packet_drop("out of quota while creating ARP packet list element"); });
			}
		);
	};
	auto without_next_hop = [&] { result = packet_drop("cannot find next hop"); };
	remote_domain.with_next_hop(dst_ip, with_next_hop, without_next_hop);
	return result;
}


Packet_result Interface::_nat_link_and_pass(Ethernet_frame         &eth,
                                            Size_guard             &size_guard,
                                            Ipv4_packet            &ip,
                                            Internet_checksum_diff &ip_icd,
                                            L3_protocol      const  prot,
                                            void            *const  prot_base,
                                            size_t           const  prot_size,
                                            Link_side_id     const &local_id,
                                            Domain                 &local_domain,
                                            Domain                 &remote_domain)
{
	Packet_result result { };
	Port_allocator_guard *remote_port_alloc_ptr { };
	remote_domain.nat_rules().find_by_domain(
		local_domain,
		[&] /* handle_match */ (Nat_rule &nat)
		{
			if(_config_ptr->verbose()) {
				log("[", local_domain, "] using NAT rule: ", nat); }

			Port src_port(0);
			nat.port_alloc(prot).alloc().with_result(
				[&] (Port src_port) {
					_src_port(prot, prot_base, src_port);
					ip.src(remote_domain.ip_config().interface().address, ip_icd);
					remote_port_alloc_ptr = &nat.port_alloc(prot); },
				[&] (auto) {
					result = packet_drop("no available NAT ports");
					switch (prot) {
					case L3_protocol::TCP: _tcp_stats.refused_for_ports++; break;
					case L3_protocol::UDP: _udp_stats.refused_for_ports++; break;
					case L3_protocol::ICMP: _icmp_stats.refused_for_ports++; break;
					default: ASSERT_NEVER_REACHED; } });
		},
		[&] /* no_match */ () { }
	);
	if (result.valid())
		return result;

	Link_side_id const remote_id = { ip.dst(), _dst_port(prot, prot_base),
	                                 ip.src(), _src_port(prot, prot_base) };
	result = _new_link(prot, prot_base, local_domain, local_id, remote_port_alloc_ptr, remote_domain, remote_id);
	if (result.valid())
		return result;

	_pass_prot_to_domain(remote_domain, eth, size_guard, ip, ip_icd, prot, prot_base, prot_size);
	return packet_handled();
}


void Interface::_send_dhcp_reply(Dhcp_server               const &dhcp_srv,
                                 Mac_address               const &eth_dst,
                                 Mac_address               const &client_mac,
                                 Ipv4_address              const &client_ip,
                                 Dhcp_packet::Message_type        msg_type,
                                 uint32_t                         xid,
                                 Ipv4_address_prefix       const &local_intf)
{
	enum { PKT_SIZE = 512 };
	send(PKT_SIZE, [&] (void *pkt_base,  Size_guard &size_guard) {

		/* create ETH header of the reply */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		if (msg_type == Dhcp_packet::Message_type::OFFER) {
			eth.dst(Ethernet_frame::broadcast()); }
		else {
			eth.dst(eth_dst); }
		eth.src(_router_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the reply */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src(local_intf.address);
		ip.dst(client_ip);

		/* create UDP header of the reply */
		size_t const udp_off = size_guard.head_size();
		Udp_packet &udp = ip.construct_at_data<Udp_packet>(size_guard);
		udp.src_port(Port(Dhcp_packet::BOOTPS));
		udp.dst_port(Port(Dhcp_packet::BOOTPC));

		/* create mandatory DHCP fields of the reply  */
		Dhcp_packet &dhcp = udp.construct_at_data<Dhcp_packet>(size_guard);
		dhcp.op(Dhcp_packet::REPLY);
		dhcp.htype(Dhcp_packet::Htype::ETH);
		dhcp.hlen(sizeof(Mac_address));
		dhcp.xid(xid);
		if (msg_type == Dhcp_packet::Message_type::INFORM) {
			dhcp.ciaddr(client_ip); }
		else {
			dhcp.yiaddr(client_ip); }
		dhcp.siaddr(local_intf.address);
		dhcp.client_mac(client_mac);
		dhcp.default_magic_cookie();

		/* append DHCP option fields to the reply */
		Dhcp_packet::Options_aggregator<Size_guard> dhcp_opts(dhcp, size_guard);
		dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
		dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(local_intf.address);
		dhcp_opts.append_option<Dhcp_packet::Ip_lease_time>(dhcp_srv.ip_lease_time().value / 1000 / 1000);
		dhcp_opts.append_option<Dhcp_packet::Subnet_mask>(local_intf.subnet_mask());
		dhcp_opts.append_option<Dhcp_packet::Router_ipv4>(local_intf.address);

		if (not dhcp_srv.dns_servers_empty()) {

			dhcp_opts.append_dns_server(
				[&] (Dhcp_options::Dns_server_data &data)
				{
					dhcp_srv.for_each_dns_server_ip(
						[&] (Ipv4_address const &addr)
						{
							data.append_address(addr);
						});
				});
		}
		dhcp_srv.dns_domain_name().with_string(
			[&] (Dns_domain_name::String const &str)
		{
			dhcp_opts.append_domain_name(str.string(), str.length());
		});
		dhcp_opts.append_option<Dhcp_packet::Broadcast_addr>(local_intf.broadcast_address());
		dhcp_opts.append_option<Dhcp_packet::Options_end>();

		/* fill in header values that need the packet to be complete already */
		udp.length(size_guard.head_size() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}


void Interface::_release_dhcp_allocation(Dhcp_allocation &allocation,
                                         Domain          &local_domain)
{
	if (_config_ptr->verbose()) {
		log("[", local_domain, "] release DHCP allocation: ", allocation); }

	_dhcp_allocations.remove(allocation);
}


Packet_result Interface::_new_dhcp_allocation(Ethernet_frame &eth,
                                              Dhcp_packet    &dhcp,
                                              Dhcp_server    &dhcp_srv,
                                              Domain         &local_domain)
{
	Packet_result result { };
	dhcp_srv.alloc_ip().with_result(
		[&] (Ipv4_address const &ip) {
			retry_once<Out_of_ram, Out_of_caps>(
				[&] {
					Dhcp_allocation &allocation = *new (_alloc)
						Dhcp_allocation { *this, ip, dhcp.client_mac(),
						                  _timer, _config_ptr->dhcp_offer_timeout() };

					_dhcp_allocations.insert(allocation);
					if (_config_ptr->verbose()) {
						log("[", local_domain, "] offer DHCP allocation: ", allocation); }

					_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid(),
					                 local_domain.ip_config().interface());

					result = packet_handled();
				},
				[&] { _try_emergency_free_quota(); },
				[&] { result = packet_drop("out of quota while creating DHCP allocation"); });
		},
		[&] (auto) { result = packet_drop("failed to allocate IP for DHCP client"); });

	return result;
}


Packet_result Interface::_handle_dhcp_request(Ethernet_frame            &eth,
                                              Dhcp_server               &dhcp_srv,
                                              Dhcp_packet               &dhcp,
                                              Domain                    &local_domain,
                                              Ipv4_address_prefix const &local_intf)
{
	Packet_result result { };
	auto no_msg_type_fn = [&] { result = packet_drop("DHCP request misses option \"Message Type\""); };
	auto msg_type_fn = [&] (Dhcp_packet::Message_type_option const &msg_type) {

		/* look up existing DHCP configuration for client */
		auto dhcp_allocation_fn = [&] (Dhcp_allocation &allocation) {

			switch (msg_type.value()) {
			case Dhcp_packet::Message_type::DISCOVER:

				if (allocation.bound()) {

					_release_dhcp_allocation(allocation, local_domain);
					_destroy_dhcp_allocation(allocation, local_domain);
					result = _new_dhcp_allocation(eth, dhcp, dhcp_srv, local_domain);
					return;

				} else {
					allocation.lifetime(_config_ptr->dhcp_offer_timeout());
					_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid(), local_intf);
					result = packet_handled();
					return;
				}
			case Dhcp_packet::Message_type::REQUEST:

				if (allocation.bound()) {
					allocation.lifetime(dhcp_srv.ip_lease_time());
					_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::ACK,
					                 dhcp.xid(), local_intf);
					result = packet_handled();
					return;

				} else {

					auto no_server_ipv4_fn = [&] { result = packet_drop("DHCP request misses option \"Server IPv4\""); };
					auto server_ipv4_fn = [&] (Dhcp_packet::Server_ipv4 const &dhcp_srv_ip) {

						if (dhcp_srv_ip.value() == local_intf.address)
						{
							allocation.set_bound();
							allocation.lifetime(dhcp_srv.ip_lease_time());
							if (_config_ptr->verbose()) {
								log("[", local_domain, "] bind DHCP allocation: ",
								    allocation);
							}
							_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
							                 allocation.ip(),
							                 Dhcp_packet::Message_type::ACK,
							                 dhcp.xid(), local_intf);
							result = packet_handled();
							return;

						} else {

							_release_dhcp_allocation(allocation, local_domain);
							_destroy_dhcp_allocation(allocation, local_domain);
							result = packet_handled();
							return;
						}
					};
					dhcp.with_option<Dhcp_packet::Server_ipv4>(server_ipv4_fn, no_server_ipv4_fn);
					return;
				}
			case Dhcp_packet::Message_type::INFORM:

				_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
				                 allocation.ip(),
				                 Dhcp_packet::Message_type::ACK,
				                 dhcp.xid(), local_intf);
				result = packet_handled();
				return;

			case Dhcp_packet::Message_type::DECLINE:
			case Dhcp_packet::Message_type::RELEASE:

				_release_dhcp_allocation(allocation, local_domain);
				_destroy_dhcp_allocation(allocation, local_domain);
				result = packet_handled();
				return;

			case Dhcp_packet::Message_type::NAK:   result = packet_drop("DHCP NAK from client"); return;
			case Dhcp_packet::Message_type::OFFER: result = packet_drop("DHCP OFFER from client"); return;
			case Dhcp_packet::Message_type::ACK:   result = packet_drop("DHCP ACK from client"); return;
			default:                               result = packet_drop("DHCP request with broken message type"); return;
			}
		};
		auto no_dhcp_allocation_fn = [&] {

			switch (msg_type.value()) {
			case Dhcp_packet::Message_type::DISCOVER:

				result = _new_dhcp_allocation(eth, dhcp, dhcp_srv, local_domain);
				return;

			case Dhcp_packet::Message_type::REQUEST:

				_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
				                 Ipv4_address { },
				                 Dhcp_packet::Message_type::NAK,
				                 dhcp.xid(), local_intf);
				result = packet_handled();
				return;

			case Dhcp_packet::Message_type::DECLINE: result = packet_drop("DHCP DECLINE from client without offered/acked IP"); return;
			case Dhcp_packet::Message_type::RELEASE: result = packet_drop("DHCP RELEASE from client without offered/acked IP"); return;
			case Dhcp_packet::Message_type::NAK:     result = packet_drop("DHCP NAK from client"); return;
			case Dhcp_packet::Message_type::OFFER:   result = packet_drop("DHCP OFFER from client"); return;
			case Dhcp_packet::Message_type::ACK:     result = packet_drop("DHCP ACK from client"); return;
			default:                                 result = packet_drop("DHCP request with broken message type"); return;
			}
		};
		_dhcp_allocations.find_by_mac(dhcp.client_mac(), dhcp_allocation_fn, no_dhcp_allocation_fn);
	};
	dhcp.with_option<Dhcp_packet::Message_type_option>(msg_type_fn, no_msg_type_fn);
	return result;
}


void Interface::_domain_broadcast(Ethernet_frame &eth,
                                  Size_guard     &size_guard,
                                  Domain         &local_domain)
{
	local_domain.interfaces().for_each([&] (Interface &interface) {
		if (&interface != this) {
			interface.send(eth, size_guard);
		}
	});
}


void Interface::_send_icmp_dst_unreachable(Ipv4_address_prefix const &local_intf,
                                           Ethernet_frame      const &req_eth,
                                           Ipv4_packet         const &req_ip,
                                           Icmp_packet::Code   const  code)
{
	enum {
		ICMP_MAX_DATA_SIZE = sizeof(Ipv4_packet) + 8,
		PKT_SIZE           = sizeof(Ethernet_frame) + sizeof(Ipv4_packet) +
		                     sizeof(Icmp_packet) + ICMP_MAX_DATA_SIZE,
	};
	send(PKT_SIZE, [&] (void *pkt_base, Size_guard &size_guard) {

		/* create ETH header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(req_eth.src());
		eth.src(_router_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header */
		size_t const ip_off = size_guard.head_size();
		Ipv4_packet &ip = eth.construct_at_data<Ipv4_packet>(size_guard);
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.protocol(Ipv4_packet::Protocol::ICMP);
		ip.src(local_intf.address);
		ip.dst(req_ip.src());

		/* create ICMP header */
		Icmp_packet &icmp = ip.construct_at_data<Icmp_packet>(size_guard);
		icmp.type(Icmp_packet::Type::DST_UNREACHABLE);
		icmp.code(code);
		size_t icmp_data_size = req_ip.total_length();
		if (icmp_data_size > ICMP_MAX_DATA_SIZE) {
			icmp_data_size = ICMP_MAX_DATA_SIZE;
		}
		/* create ICMP data */
		icmp.copy_to_data(&req_ip, icmp_data_size, size_guard);

		/* fill in header values that require the packet to be complete */
		icmp.update_checksum(icmp_data_size);
		ip.total_length(size_guard.head_size() - ip_off);
		ip.update_checksum();
	});
}


bool Interface::link_state() const
{
	return _policy.interface_link_state();
}


void Interface::handle_interface_link_state()
{
	attach_to_domain_finish();

	/* discard dynamic IP config if the entire domain is down */
	with_domain([&] (Domain &domain) {
		if (domain.ip_config_dynamic() && !link_state() && domain.ip_config().valid()) {
			bool discard_ip_config = true;
			domain.interfaces().for_each([&] (Interface &interface) {
				if (interface.link_state())
					discard_ip_config = false; });
			if (discard_ip_config) {
				domain.discard_ip_config();
				domain.arp_cache().destroy_all_entries();
			}
		}
	});
	/* force report if configured */
	_config_ptr->with_report([&] (Report &r) { r.handle_interface_link_state(); });
}


void Interface::_send_icmp_echo_reply(Ethernet_frame &eth,
                                      Ipv4_packet    &ip,
                                      Icmp_packet    &icmp,
                                      size_t          icmp_sz,
                                      Size_guard     &size_guard)
{
	/* adapt Ethernet header */
	Mac_address const eth_src = eth.src();
	eth.src(eth.dst());
	eth.dst(eth_src);

	/* adapt IPv4 header */
	Ipv4_address const ip_src = ip.src();
	ip.src(ip.dst());
	ip.dst(ip_src);

	/* adapt ICMP header */
	icmp.type(Icmp_packet::Type::ECHO_REPLY);
	icmp.code(Icmp_packet::Code::ECHO_REPLY);

	/*
	 * Update checksums and send
	 *
	 * Skip updating the IPv4 checksum because we have only swapped SRC and
	 * DST and these changes cancel each other out in checksum calculation.
	 */
	icmp.update_checksum(icmp_sz - sizeof(Icmp_packet));
	send(eth, size_guard);
}


Packet_result Interface::_handle_icmp_query(Ethernet_frame          &eth,
                                            Size_guard              &size_guard,
                                            Ipv4_packet             &ip,
                                            Internet_checksum_diff  &ip_icd,
                                            Packet_descriptor const &pkt,
                                            L3_protocol              prot,
                                            void                    *prot_base,
                                            size_t                   prot_size,
                                            Domain                  &local_domain)
{
	Packet_result result { };
	Link_side_id const local_id = { ip.src(), _src_port(prot, prot_base),
	                                ip.dst(), _dst_port(prot, prot_base) };

	/* try to route via existing ICMP links */
	local_domain.links(prot).find_by_id(
		local_id,
		[&] /* handle_match */ (Link_side const &local_side)
		{
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Domain &remote_domain = remote_side.domain();
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] using ", l3_protocol_name(prot),
				    " link: ", link);
			}
			result = _adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
			if (result.valid())
				return;
			ip.src(remote_side.dst_ip(), ip_icd);
			ip.dst(remote_side.src_ip(), ip_icd);
			_src_port(prot, prot_base, remote_side.dst_port());
			_dst_port(prot, prot_base, remote_side.src_port());
			_pass_prot_to_domain(
				remote_domain, eth, size_guard, ip, ip_icd, prot,
				prot_base, prot_size);

			_link_packet(prot, prot_base, link, client);
			result = packet_handled();
		},
		[&] /* handle_no_match */ () { }
	);
	if (result.valid())
		return result;

	/* try to route via ICMP rules */
	local_domain.icmp_rules().find_longest_prefix_match(
		ip.dst(),
		[&] /* handle_match */ (Ip_rule const &rule)
		{
			if(_config_ptr->verbose()) {
				log("[", local_domain, "] using ICMP rule: ", rule); }

			Domain &remote_domain = rule.domain();
			result = _adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
			if (result.valid())
				return;
			result = _nat_link_and_pass(
				eth, size_guard, ip, ip_icd, prot, prot_base, prot_size, local_id, local_domain, remote_domain);
		},
		[&] /* handle_no_match */ () { }
	);
	return result;
}


Packet_result Interface::_handle_icmp_error(Ethernet_frame          &eth,
                                            Size_guard              &size_guard,
                                            Ipv4_packet             &ip,
                                            Internet_checksum_diff  &ip_icd,
                                            Packet_descriptor const &pkt,
                                            Domain                  &local_domain,
                                            Icmp_packet             &icmp,
                                            size_t                   icmp_sz)
{
	Packet_result result { };
	Ipv4_packet            &embed_ip     { icmp.data<Ipv4_packet>(size_guard) };
	Internet_checksum_diff  embed_ip_icd { };

	/* drop packet if embedded IP checksum invalid */
	if (embed_ip.checksum_error()) {
		return packet_drop("bad checksum in IP packet embedded in ICMP error");
	}
	/* get link identity of the embeddeded transport packet */
	L3_protocol  const embed_prot      = embed_ip.protocol();
	void        *const embed_prot_base = _prot_base(embed_prot, size_guard, embed_ip);
	Link_side_id const local_id = { embed_ip.dst(), _dst_port(embed_prot, embed_prot_base),
	                                embed_ip.src(), _src_port(embed_prot, embed_prot_base) };

	/* lookup a link state that matches the embedded transport packet */
	local_domain.links(embed_prot).find_by_id(
		local_id,
		[&] /* handle_match */ (Link_side const &local_side)
		{
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Domain &remote_domain = remote_side.domain();

			/* print out that the link is used */
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] using ",
				    l3_protocol_name(embed_prot), " link: ", link);
			}
			/* adapt source and destination of Ethernet frame and IP packet */
			result = _adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
			if (result.valid())
				return;
			if (remote_side.dst_ip() == remote_domain.ip_config().interface().address) {
				ip.src(remote_side.dst_ip(), ip_icd);
			}
			ip.dst(remote_side.src_ip(), ip_icd);

			/* adapt source and destination of embedded IP and transport packet */
			embed_ip.src(remote_side.src_ip(), embed_ip_icd);
			embed_ip.dst(remote_side.dst_ip(), embed_ip_icd);
			_src_port(embed_prot, embed_prot_base, remote_side.src_port());
			_dst_port(embed_prot, embed_prot_base, remote_side.dst_port());

			/* update checksum of both IP headers and the ICMP header */
			embed_ip.update_checksum(embed_ip_icd);
			icmp.update_checksum(icmp_sz - sizeof(Icmp_packet));
			ip.update_checksum(ip_icd);

			/* send adapted packet to all interfaces of remote domain */
			remote_domain.interfaces().for_each([&] (Interface &interface) {
				interface.send(eth, size_guard);
			});
			/* refresh link only if the error is not about an ICMP query */
			if (embed_prot != L3_protocol::ICMP) {
				_link_packet(embed_prot, embed_prot_base, link, client); }
			result = packet_handled();
		},
		[&] /* handle_no_match */ ()
		{
			result = packet_drop("no link that matches packet embedded in ICMP error");
		}
	);
	return result;
}


Packet_result Interface::_handle_icmp(Ethernet_frame            &eth,
                                      Size_guard                &size_guard,
                                      Ipv4_packet               &ip,
                                      Internet_checksum_diff    &ip_icd,
                                      Packet_descriptor   const &pkt,
                                      L3_protocol                prot,
                                      void                      *prot_base,
                                      size_t                     prot_size,
                                      Domain                    &local_domain,
                                      Ipv4_address_prefix const &local_intf)
{
	/* drop packet if ICMP checksum is invalid */
	Packet_result result { };
	Icmp_packet &icmp = *reinterpret_cast<Icmp_packet *>(prot_base);
	if (icmp.checksum_error(size_guard.unconsumed())) {
		return packet_drop("bad ICMP checksum"); }

	/* try to act as ICMP Echo server */
	if (icmp.type() == Icmp_packet::Type::ECHO_REQUEST &&
	    ip.dst() == local_intf.address &&
	    local_domain.icmp_echo_server())
	{
		if(_config_ptr->verbose()) {
			log("[", local_domain, "] act as ICMP Echo server"); }

		_send_icmp_echo_reply(eth, ip, icmp, prot_size, size_guard);
		return packet_handled();
	}
	/* try to act as ICMP router */
	switch (icmp.type()) {
	case Icmp_packet::Type::ECHO_REPLY:
	case Icmp_packet::Type::ECHO_REQUEST: result = _handle_icmp_query(eth, size_guard, ip, ip_icd, pkt, prot, prot_base, prot_size, local_domain); break;
	case Icmp_packet::Type::DST_UNREACHABLE: result = _handle_icmp_error(eth, size_guard, ip, ip_icd, pkt, local_domain, icmp, prot_size); break;
	default: result = packet_drop("unhandled type in ICMP"); }
	return result;
}


Packet_result Interface::_handle_ip(Ethernet_frame          &eth,
                                    Size_guard              &size_guard,
                                    Packet_descriptor const &pkt,
                                    Domain                  &local_domain)
{
	Packet_result result { };
	Ipv4_packet            &ip     { eth.data<Ipv4_packet>(size_guard) };
	Internet_checksum_diff  ip_icd { };

	/* drop fragmented IPv4 as it isn't supported */
	Ipv4_address_prefix const &local_intf = local_domain.ip_config().interface();
	if (ip.more_fragments() ||
	    ip.fragment_offset() != 0) {

		_dropped_fragm_ipv4++;
		if (_config_ptr->icmp_type_3_code_on_fragm_ipv4() != Icmp_packet::Code::INVALID) {
			_send_icmp_dst_unreachable(
				local_intf, eth, ip, _config_ptr->icmp_type_3_code_on_fragm_ipv4());
		}
		return packet_drop("fragmented IPv4 not supported");
	}
	/* try handling subnet-local IP packets */
	if (local_intf.prefix_matches(ip.dst()) &&
	    ip.dst() != local_intf.address)
	{
		/*
		 * Packet targets IP local to the domain's subnet and doesn't target
		 * the router. Thus, forward it to all other interfaces of the domain.
		 */
		_domain_broadcast(eth, size_guard, local_domain);
		return packet_handled();
	}

	/* try to route via transport layer rules */
	L3_protocol const prot = ip.protocol();
	if (_supported_transport_prot(prot)) {

		size_t const prot_size = size_guard.unconsumed();
		void *const prot_base = _prot_base(prot, size_guard, ip);

		/* try handling DHCP requests before trying any routing */
		if (prot == L3_protocol::UDP) {
			Udp_packet &udp = *reinterpret_cast<Udp_packet *>(prot_base);

			if (Dhcp_packet::is_dhcp(&udp)) {

				/* get DHCP packet */
				Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
				switch (dhcp.op()) {
				case Dhcp_packet::REQUEST:

					local_domain.with_dhcp_server(
						[&] /* dhcp_server_fn */ (Dhcp_server &srv) {
							result = _handle_dhcp_request(eth, srv, dhcp, local_domain, local_intf); },
						[&] /* no_dhcp_server_fn */ {
							result = packet_drop("DHCP request while DHCP server inactive"); });
					return result;

				case Dhcp_packet::REPLY:

					if (eth.dst() != router_mac() &&
					    eth.dst() != Mac_address(0xff))
					{
						return packet_drop("Ethernet of DHCP reply doesn't target router"); }

					if (dhcp.client_mac() != router_mac()) {
						return packet_drop("DHCP reply doesn't target router"); }

					if (!_dhcp_client.constructed()) {
						return packet_drop("DHCP reply while DHCP client inactive"); }

					return _dhcp_client->handle_dhcp_reply(dhcp, local_domain);

				default: return packet_drop("Bad DHCP opcode");
				}
			}
		}
		if (prot == L3_protocol::ICMP) {
			result = _handle_icmp(eth, size_guard, ip, ip_icd, pkt, prot, prot_base,
			                      prot_size, local_domain, local_intf);
		} else {

			Link_side_id const local_id = { ip.src(), _src_port(prot, prot_base),
			                                ip.dst(), _dst_port(prot, prot_base) };

			/* try to route via existing UDP/TCP links */
			local_domain.links(prot).find_by_id(
				local_id,
				[&] /* handle_match */ (Link_side const &local_side)
				{
					Link &link = local_side.link();
					bool const client = local_side.is_client();
					Link_side &remote_side =
						client ? link.server() : link.client();

					Domain &remote_domain = remote_side.domain();
					if (_config_ptr->verbose()) {
						log("[", local_domain, "] using ", l3_protocol_name(prot),
						    " link: ", link);
					}
					result = _adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
					if (result.valid())
						return;
					ip.src(remote_side.dst_ip(), ip_icd);
					ip.dst(remote_side.src_ip(), ip_icd);
					_src_port(prot, prot_base, remote_side.dst_port());
					_dst_port(prot, prot_base, remote_side.src_port());
					_pass_prot_to_domain(
						remote_domain, eth, size_guard, ip, ip_icd, prot,
						prot_base, prot_size);

					_link_packet(prot, prot_base, link, client);
					result = packet_handled();
				},
				[&] /* handle_no_match */ () { }
			);
			if (result.valid())
				return result;

			/* try to route via forward rules */
			if (local_id.dst_ip == local_intf.address) {
				_with_forward_rule(local_domain, prot, local_id.dst_port, [&] (Forward_rule const &rule) {
					if(_config_ptr->verbose()) {
						log("[", local_domain, "] using forward rule: ",
							l3_protocol_name(prot), " ", rule);
					}
					Domain &remote_domain = rule.domain();
					result = _adapt_eth(eth, rule.to_ip(), pkt, remote_domain);
					if (result.valid())
						return;
					ip.dst(rule.to_ip(), ip_icd);
					if (!(rule.to_port() == Port(0))) {
						_dst_port(prot, prot_base, rule.to_port());
					}
					result = _nat_link_and_pass(
						eth, size_guard, ip, ip_icd, prot, prot_base,
						prot_size, local_id, local_domain, remote_domain);
				});
				if (result.valid())
					return result;
			}
			/* try to route via transport and permit rules */
			_with_transport_rule(local_domain, prot, local_id.dst_ip, local_id.dst_port,
				[&] (Transport_rule const &transport_rule, Permit_rule const &permit_rule) {
					if(_config_ptr->verbose()) {
						log("[", local_domain, "] using ",
							l3_protocol_name(prot), " rule: ",
							transport_rule, " ", permit_rule);
					}
					Domain &remote_domain = permit_rule.domain();
					result = _adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
					if (result.valid())
						return;
					result = _nat_link_and_pass(
						eth, size_guard, ip, ip_icd, prot, prot_base, prot_size,
						local_id, local_domain, remote_domain);
				});
		}
	}
	if (result.valid())
		return result;

	/* try to route via IP rules */
	local_domain.ip_rules().find_longest_prefix_match(
		ip.dst(),
		[&] /* handle_match */ (Ip_rule const &rule)
		{
			if(_config_ptr->verbose()) {
				log("[", local_domain, "] using IP rule: ", rule); }

			Domain &remote_domain = rule.domain();
			result = _adapt_eth(eth, ip.dst(), pkt, remote_domain);
			if (result.valid())
				return;
			remote_domain.interfaces().for_each([&] (Interface &interface) {
				interface.send(eth, size_guard);
			});
			result = packet_handled();
		},
		[&] /* handle_no_match */ () { }
	);
	if (result.valid())
		return result;

	/*
	 * Give up and drop packet. According to RFC 1812 section 4.3.2.7, an ICMP
	 * "Destination Unreachable" is sent as response only if the dropped
	 * packet fullfills certain properties.
	 *
	 * FIXME
	 *
	 * There are some properties required by the RFC that are not yet checked
	 * at this point.
	 */
	if(not ip.dst().is_multicast()) {

		_send_icmp_dst_unreachable(local_intf, eth, ip,
		                           Icmp_packet::Code::DST_HOST_UNREACHABLE);
	}
	return packet_drop("unroutable");
}


void Interface::_broadcast_arp_request(Ipv4_address const &src_ip,
                                       Ipv4_address const &dst_ip)
{
	send(ARP_PACKET_SIZE, [&] (void *pkt_base, Size_guard &size_guard) {

		/* write Ethernet header */
		Ethernet_frame &eth = Ethernet_frame::construct_at(pkt_base, size_guard);
		eth.dst(Mac_address(0xff));
		eth.src(_router_mac);
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = eth.construct_at_data<Arp_packet>(size_guard);
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REQUEST);
		arp.src_mac(_router_mac);
		arp.src_ip(src_ip);
		arp.dst_mac(Mac_address(0xff));
		arp.dst_ip(dst_ip);
	});
}


void Interface::_handle_arp_reply(Ethernet_frame &eth,
                                  Size_guard     &size_guard,
                                  Arp_packet     &arp,
                                  Domain         &local_domain)
{
	local_domain.arp_cache().find_by_ip(
		arp.src_ip(),
		[&] /* handle_match */ (Arp_cache_entry const &)
		{
			/* check wether a matching ARP cache entry already exists */
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] ARP entry already exists"); }
		},
		[&] /* handle_no_match */ ()
		{
			/* by now, no matching ARP cache entry exists, so create one */
			Ipv4_address const ip = arp.src_ip();
			local_domain.arp_cache().new_entry(ip, arp.src_mac());

			/* continue handling of packets that waited for the entry */
			for (Arp_waiter_list_element *waiter_le = local_domain.foreign_arp_waiters().first();
			     waiter_le; )
			{
				Arp_waiter &waiter = *waiter_le->object();
				waiter_le = waiter_le->next();
				if (ip != waiter.ip()) { continue; }
				waiter.flush_packets(waiter.src()._alloc, [&] (Packet_descriptor const &packet) {
					waiter.src()._continue_handle_eth(packet); });
				destroy(waiter.src()._alloc, &waiter);
			}
		}
	);
	Ipv4_address_prefix const &local_intf = local_domain.ip_config().interface();
	if (local_intf.prefix_matches(arp.dst_ip()) &&
	    arp.dst_ip() != local_intf.address)
	{
		/*
		 * Packet targets IP local to the domain's subnet and doesn't target
		 * the router. Thus, forward it to all other interfaces of the domain.
		 */
		if (_config_ptr->verbose()) {
			log("[", local_domain, "] forward ARP reply for local IP "
			    "to all interfaces of the sender domain"); }
		_domain_broadcast(eth, size_guard, local_domain);
	}
}


void Interface::_send_arp_reply(Ethernet_frame &request_eth,
                                Arp_packet     &request_arp)
{
	send(ARP_PACKET_SIZE, [&] (void *reply_base, Size_guard &reply_guard) {

		Ethernet_frame &reply_eth {
			Ethernet_frame::construct_at(reply_base, reply_guard) };

		reply_eth.dst(request_eth.src());
		reply_eth.src(_router_mac);
		reply_eth.type(Ethernet_frame::Type::ARP);

		Arp_packet &reply_arp {
			reply_eth.construct_at_data<Arp_packet>(reply_guard) };

		reply_arp.hardware_address_type(Arp_packet::ETHERNET);
		reply_arp.protocol_address_type(Arp_packet::IPV4);
		reply_arp.hardware_address_size(sizeof(Mac_address));
		reply_arp.protocol_address_size(sizeof(Ipv4_address));
		reply_arp.opcode(Arp_packet::REPLY);
		reply_arp.src_mac(_router_mac);
		reply_arp.src_ip(request_arp.dst_ip());
		reply_arp.dst_mac(request_arp.src_mac());
		reply_arp.dst_ip(request_arp.src_ip());
	});
}


Packet_result Interface::_handle_arp_request(Ethernet_frame &eth,
                                             Size_guard     &size_guard,
                                             Arp_packet     &arp,
                                             Domain         &local_domain)
{
	Ipv4_config         const &local_ip_cfg = local_domain.ip_config();
	Ipv4_address_prefix const &local_intf   = local_ip_cfg.interface();
	if (local_intf.prefix_matches(arp.dst_ip())) {

		/* ARP request for an IP local to the domain's subnet */
		if (arp.src_ip() == arp.dst_ip()) {

			/* gratuitous ARP requests are not really necessary */
			return packet_drop("gratuitous ARP request");

		} else if (arp.dst_ip() == local_intf.address) {

			/* ARP request for the routers IP at this domain */
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] answer ARP request for router IP "
				    "with router MAC"); }
			_send_arp_reply(eth, arp);

		} else {

			/* forward request to all other interfaces of the domain */
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] forward ARP request for local IP "
				    "to all interfaces of the sender domain"); }
			_domain_broadcast(eth, size_guard, local_domain);
		}

	} else {

		/* ARP request for an IP foreign to the domain's subnet */
		if (local_ip_cfg.gateway_valid()) {

			/* leave request up to the gateway of the domain */
			return packet_drop("leave ARP request up to gateway");

		} else {

			/* try to act as gateway for the domain as none is configured */
			if (_config_ptr->verbose()) {
				log("[", local_domain, "] answer ARP request for foreign IP "
				    "with router MAC"); }
			_send_arp_reply(eth, arp);
		}
	}
	return packet_handled();
}


Packet_result Interface::_handle_arp(Ethernet_frame &eth,
                                     Size_guard     &size_guard,
                                     Domain         &local_domain)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	Arp_packet &arp = eth.data<Arp_packet>(size_guard);
	if (!arp.ethernet_ipv4()) {
		return packet_drop("ARP for unknown protocol"); }

	switch (arp.opcode()) {
	case Arp_packet::REPLY: _handle_arp_reply(eth, size_guard, arp, local_domain); break;
	case Arp_packet::REQUEST: return _handle_arp_request(eth, size_guard, arp, local_domain);
	default: return packet_drop("unknown ARP operation"); }
	return packet_handled();
}


void Interface::_drop_packet(Packet_descriptor const &pkt, char const *reason)
{
	_ack_packet(pkt);
	with_domain(
		[&] /* domain_fn */ (Domain &domain) {
			if (domain .verbose_packet_drop())
				log("[", domain, "] drop packet (", reason, ")"); },
		[&] /* no_domain_fn */ {
			if (_config_ptr->verbose())
				log("[?] drop packet (", reason, ")"); });
}


void Interface::_handle_pkt()
{
	Packet_descriptor const pkt = _sink.get_packet();
	if (!_sink.packet_valid(pkt) || pkt.size() < sizeof(Packet_stream_sink::Content_type)) {
		_drop_packet(pkt, "invalid Nic packet");
		return;
	}
	Size_guard size_guard(pkt.size());
	Packet_result result = _handle_eth(_sink.packet_content(pkt), size_guard, pkt);
	switch (result.type) {
	case Packet_result::HANDLED: _ack_packet(pkt); break;
	case Packet_result::POSTPONED: break;
	case Packet_result::DROP: _drop_packet(pkt, result.drop_reason); break;
	case Packet_result::INVALID: ASSERT_NEVER_REACHED; }
}


void Interface::_handle_pkt_stream_signal()
{
	_timer.update_cached_time();

	/*
	 * Release all sent packets that were already acknowledged by the counter
	 * side. Doing this first frees packet-stream memory which facilitates
	 * sending new packets in the subsequent steps of this handler.
	 */
	while (_source.ack_avail()) {
		_source.release_packet(_source.try_get_acked_packet());
	}

	/*
	 * Handle packets received from the counter side. If the user configured
	 * a limit for the number of packets to be handled at once, this limit gets
	 * applied. If there is no such limit, received packets are handled until
	 * none is left.
	 */
	unsigned long const max_pkts = _config_ptr->max_packets_per_signal();
	if (max_pkts) {
		for (unsigned long i = 0; _sink.packet_avail(); i++) {

			if (i >= max_pkts) {

				/*
				 * Ensure that this handler is called again in order to handle
				 * the packets left unhandled due to the configured limit.
				 */
				Signal_transmitter(_pkt_stream_signal_handler).submit();
				break;
			}
			_handle_pkt();
		}
	} else {
		while (_sink.packet_avail()) {
			_handle_pkt();
		}
	}

	/*
	 * Since we use the try_*() variants of the packet-stream API, we
	 * haven't emitted any packet_avail, ack_avail, ready_to_submit or
	 * ready_to_ack signal up to now. We've removed packets from our sink's
	 * submit queue and might have forwarded it to any interface. We may have
	 * also removed acks from our sink's ack queue.
	 *
	 * We therefore wakeup all sources and our sink. Note that the packet-stream
	 * API takes care of emitting only the signals that are actually needed.
	 */
	_config_ptr->domains().for_each([&] (Domain &domain) {
		domain.interfaces().for_each([&] (Interface &interface) {
			interface.wakeup_source();
		});
	});
	wakeup_sink();
}


void Interface::_continue_handle_eth(Packet_descriptor const &pkt)
{
	if (!_sink.packet_valid(pkt) || pkt.size() < sizeof(Packet_stream_sink::Content_type)) {
		_drop_packet(pkt, "invalid Nic packet");
		return;
	}
	Size_guard size_guard(pkt.size());
	Packet_result result = _handle_eth(_sink.packet_content(pkt), size_guard, pkt);
	switch (result.type) {
	case Packet_result::HANDLED: _ack_packet(pkt); break;
	case Packet_result::POSTPONED: _drop_packet(pkt, "postponed twice"); break;
	case Packet_result::DROP: _drop_packet(pkt, result.drop_reason); break;
	case Packet_result::INVALID: ASSERT_NEVER_REACHED; }
}


void Interface::_destroy_dhcp_allocation(Dhcp_allocation &allocation,
                                         Domain          &local_domain)
{
	local_domain.with_optional_dhcp_server([&] (Dhcp_server &srv) {
		srv.free_ip(allocation.ip()); });
	destroy(_alloc, &allocation);
}


void Interface::_destroy_released_dhcp_allocations(Domain &local_domain)
{
	while (Dhcp_allocation *allocation = _released_dhcp_allocations.first()) {
		_released_dhcp_allocations.remove(allocation);
		_destroy_dhcp_allocation(*allocation, local_domain);
	}
}


void Interface::_destroy_timed_out_arp_waiters()
{
	while (Arp_waiter_list_element *le = _timed_out_arp_waiters.first()) {
		Arp_waiter &waiter = *le->object();
		waiter.flush_packets(_alloc, [&] (Packet_descriptor const &packet) {
			_drop_packet(packet, "ARP request timed out"); });
		_timed_out_arp_waiters.remove(le);
		destroy(_alloc, &waiter);
	}
}


Packet_result Interface::_handle_eth(Ethernet_frame           &eth,
                                     Size_guard               &size_guard,
                                     Packet_descriptor  const &pkt,
                                     Domain                   &local_domain)
{
	if (local_domain.ip_config().valid()) {

		switch (eth.type()) {
		case Ethernet_frame::Type::ARP: return _handle_arp(eth, size_guard, local_domain);
		case Ethernet_frame::Type::IPV4: return _handle_ip(eth, size_guard, pkt, local_domain);
		default: return packet_drop("unknown network layer protocol"); }

	} else {

		switch (eth.type()) {
		case Ethernet_frame::Type::IPV4: {

			if (eth.dst() != router_mac() &&
			    eth.dst() != Mac_address(0xff))
			{
				return packet_drop("Expecting Ethernet targeting the router"); }

			Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
			if (ip.protocol() != Ipv4_packet::Protocol::UDP) {
				return packet_drop("Expecting UDP packet"); }

			Udp_packet &udp = ip.data<Udp_packet>(size_guard);
			if (!Dhcp_packet::is_dhcp(&udp)) {
				return packet_drop("Expecting DHCP packet"); }

			Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
			switch (dhcp.op()) {
			case Dhcp_packet::REPLY:

				if (dhcp.client_mac() != router_mac()) {
					return packet_drop("Expecting DHCP targeting the router"); }

				if (!_dhcp_client.constructed()) {
					return packet_drop("Expecting DHCP client to be active"); }

				return _dhcp_client->handle_dhcp_reply(dhcp, local_domain);

			default:

				return packet_drop("Expecting DHCP reply");
			}
			break;
		}
		case Ethernet_frame::Type::ARP: {
				return packet_drop("Ignore ARP request on unconfigured interface");
		}
		default: return packet_drop("unknown network layer protocol");
		}
	}
	ASSERT_NEVER_REACHED;
}


Packet_result Interface::_handle_eth(void              *const  eth_base,
                                     Size_guard               &size_guard,
                                     Packet_descriptor  const &pkt)
{
	Packet_result result { };
	try {
		Ethernet_frame &eth = Ethernet_frame::cast_from(eth_base, size_guard);
		auto domain_fn = [&] (Domain &domain) {

			domain.raise_rx_bytes(size_guard.total_size());

			/* do garbage collection over transport-layer links and DHCP allocations */
			_destroy_dissolved_links<Icmp_link>(_dissolved_icmp_links, _alloc);
			_destroy_dissolved_links<Udp_link>(_dissolved_udp_links, _alloc);
			_destroy_dissolved_links<Tcp_link>(_dissolved_tcp_links, _alloc);
			_destroy_timed_out_arp_waiters();
			_destroy_released_dhcp_allocations(domain);

			/* log received packet if desired */
			if (domain.verbose_packets()) {
				log("[", domain, "] rcv ", eth); }

			if (domain.trace_packets())
				Genode::Trace::Ethernet_packet(
					domain.name().string(), Genode::Trace::Ethernet_packet::Direction::RECV,
						eth_base, size_guard.total_size());

			result = _handle_eth(eth, size_guard, pkt, domain);
		};
		auto no_domain_fn = [&] /* no_domain_fn */ {
			if (_config_ptr->verbose_packets())
				log("[?] rcv ", eth);
			result = packet_drop("no domain");
		};
		with_domain(domain_fn, no_domain_fn);
	}
	catch (Size_guard::Exceeded) { result = packet_drop("packet size-guard exceeded"); }
	return result;
}


void Interface::send(Ethernet_frame &eth,
                     Size_guard     &size_guard)
{
	send(size_guard.total_size(), [&] (void *pkt_base, Size_guard &size_guard) {
		Genode::memcpy(pkt_base, (void *)&eth, size_guard.total_size());
	});
}


void Interface::_send_submit_pkt(Packet_descriptor &pkt,
                                 void            * &pkt_base,
                                 size_t             pkt_size)
{
	Domain &local_domain = *_domain_ptr;
	local_domain.raise_tx_bytes(pkt_size);
	if (local_domain.verbose_packets()) {
		try {
			Size_guard size_guard(pkt_size);
			log("[", local_domain, "] snd ",
			    Ethernet_frame::cast_from(pkt_base, size_guard));
		}
		catch (Size_guard::Exceeded) { log("[", local_domain, "] snd ?"); }
	}

	if (local_domain.trace_packets())
		Genode::Trace::Ethernet_packet(local_domain.name().string(),
		                               Genode::Trace::Ethernet_packet::Direction::SENT,
		                               pkt_base,
		                               pkt_size);

	_source.try_submit_packet(pkt);
}


Interface::Interface(Genode::Entrypoint     &ep,
                     Cached_timer           &timer,
                     Mac_address      const  router_mac,
                     Genode::Allocator      &alloc,
                     Mac_address      const  mac,
                     Configuration          &config,
                     Interface_list         &interfaces,
                     Packet_stream_sink     &sink,
                     Packet_stream_source   &source,
                     Interface_policy       &policy)
:
	_sink                      { sink },
	_source                    { source },
	_pkt_stream_signal_handler { ep, *this, &Interface::_handle_pkt_stream_signal },
	_router_mac                { router_mac },
	_mac                       { mac },
	_config_ptr                { &config },
	_policy                    { policy },
	_timer                     { timer },
	_alloc                     { alloc },
	_interfaces                { interfaces }
{
	_interfaces.insert(this);
	_config_ptr->with_report([&] (Report &r) { r.handle_interface_link_state(); });
}


void Interface::_dismiss_link(Link &link)
{
	if (_config_ptr->verbose()) {
		log("[", link.client().domain(), "] dismiss link client: ", link.client());
		log("[", link.server().domain(), "] dismiss link server: ", link.server());
	}
	destroy_link(link);
}


bool Interface::_try_update_link(Link        &link,
                                 Domain      &new_srv_dom,
                                 L3_protocol  prot,
                                 Domain      &cln_dom)
{
	Domain &old_srv_dom { link.server().domain() };
	if (old_srv_dom.name()      != new_srv_dom.name() ||
	    old_srv_dom.ip_config() != new_srv_dom.ip_config())
		return false;

	if (link.client().src_ip() == link.server().dst_ip()) {
		link.handle_config(cln_dom, new_srv_dom, nullptr, *_config_ptr);
		return true;
	}
	if (link.server().dst_ip() != new_srv_dom.ip_config().interface().address)
		return false;

	bool keep_link { false };
	new_srv_dom.nat_rules().find_by_domain(
		cln_dom,
		[&] /* handle_match */ (Nat_rule &nat)
		{
			Port_allocator_guard &remote_port_alloc { nat.port_alloc(prot) };
			if (!remote_port_alloc.alloc(link.server().dst_port()))
				return;

			link.handle_config(cln_dom, new_srv_dom, &remote_port_alloc, *_config_ptr);
			keep_link = true;
		},
		[&] /* handle_no_match */ () { }
	);
	return keep_link;
}


void Interface::_update_udp_tcp_links(L3_protocol  prot,
                                      Domain      &cln_dom)
{
	links(prot).for_each([&] (Link &link) {

		bool link_updated { false };

		if (link.server().src_ip() != link.client().dst_ip()) {

			_with_forward_rule(cln_dom, prot, link.client().dst_port(), [&] (Forward_rule const &rule) {
				if (rule.to_ip() != link.server().src_ip())
					return;

				if (rule.to_port() == Port { 0 }) {

					if (link.server().src_port() !=
						link.client().dst_port())
						return;

				} else {

					if (rule.to_port() != link.server().src_port())
						return;
				}
				link_updated = _try_update_link(link, rule.domain(), prot, cln_dom);
			});

		} else {

			_with_transport_rule(cln_dom, prot, link.client().dst_ip(), link.client().dst_port(),
				[&] (Transport_rule const &, Permit_rule const &rule) {
					link_updated = _try_update_link(link, rule.domain(), prot, cln_dom); });
		}
		if (link_updated)
			return;

		_dismiss_link(link);
	});
}


void Interface::_update_icmp_links(Domain &cln_dom)
{
	L3_protocol const prot { L3_protocol::ICMP };
	links(prot).for_each([&] (Link &link) {

		bool link_has_been_updated { false };

		cln_dom.icmp_rules().find_longest_prefix_match(
			link.client().dst_ip(),
			[&] /* handle_match */ (Ip_rule const &rule)
			{
				link_has_been_updated =
					_try_update_link(link, rule.domain(), prot, cln_dom);
			},
			[&] /* handle_no_match */ () { }
		);
		if (link_has_been_updated)
			return;

		_dismiss_link(link);
	});
}


void Interface::_update_dhcp_allocations(Domain &old_domain,
                                         Domain &new_domain)
{
	bool dhcp_clients_outdated { false };

	auto dismiss_all_fn = [&] () {
		dhcp_clients_outdated = true;
		while (Dhcp_allocation *allocation = _dhcp_allocations.first()) {
			if (_config_ptr->verbose())
				log("[", new_domain, "] dismiss DHCP allocation: ",
					*allocation, " (other/no DHCP server)");
			_dhcp_allocations.remove(*allocation);
			_destroy_dhcp_allocation(*allocation, old_domain);
		}
	};

	old_domain.with_dhcp_server([&] (Dhcp_server &old_dhcp_srv) {
	new_domain.with_dhcp_server([&] (Dhcp_server &new_dhcp_srv) {
		if (old_dhcp_srv.config_equal_to_that_of(new_dhcp_srv)) {
			/* try to re-use existing DHCP allocations */
			_dhcp_allocations.for_each([&] (Dhcp_allocation &allocation) {
				if (!new_dhcp_srv.alloc_ip(allocation.ip())) {
					if (_config_ptr->verbose())
						log("[", new_domain, "] dismiss DHCP allocation: ", allocation, " (no IP)");

					dhcp_clients_outdated = true;
					_dhcp_allocations.remove(allocation);
					_destroy_dhcp_allocation(allocation, old_domain);
					return;
				}
				if (_config_ptr->verbose())
					log("[", new_domain, "] update DHCP allocation: ", allocation);
			});
		} else {
			/* dismiss all DHCP allocations */
			dismiss_all_fn();
		}
	},
	/* no DHCP server on new domain */
	[&] () { dismiss_all_fn(); }); },
	/* no DHCP server on old domain */
	[&] () { dismiss_all_fn(); });

	if (dhcp_clients_outdated)
		_reset_and_refetch_domain_ready_state();
}


void Interface::_update_own_arp_waiters(Domain &domain)
{
	bool const verbose { _config_ptr->verbose() };
	_own_arp_waiters.for_each([&] (Arp_waiter_list_element &le)
	{
		Arp_waiter &arp_waiter { *le.object() };
		bool dismiss_arp_waiter { true };
		_config_ptr->domains().with_element(
			arp_waiter.dst().name(),
			[&] /* match_fn */ (Domain &dst)
			{
				/* dismiss ARP waiter if IP config of target domain changed */
				if (dst.ip_config() != arp_waiter.dst().ip_config()) {
					return;
				}
				/* keep ARP waiter */
				arp_waiter.handle_config(dst);
				if (verbose) {
					log("[", domain, "] update ARP waiter: ", arp_waiter);
				}
				dismiss_arp_waiter = false;
			},
			[&] /* no_match_fn */ ()
			{
				/* dismiss ARP waiter as the target domain disappeared */
			}
		);
		if (dismiss_arp_waiter) {
			if (verbose) {
				log("[", domain, "] dismiss ARP waiter: ", arp_waiter);
			}
			cancel_arp_waiting(*_own_arp_waiters.first()->object());
		}
	});
}


void Interface::handle_config_1(Configuration &config)
{
	/* update config and policy */
	_config_ptr = &config;
	_policy.handle_config(config);
	Domain_name const &new_domain_name = _policy.determine_domain_name();
	with_domain([&] (Domain &old_domain) {

		/* destroy state objects that are not needed anymore */
		_destroy_dissolved_links<Icmp_link>(_dissolved_icmp_links, _alloc);
		_destroy_dissolved_links<Udp_link> (_dissolved_udp_links,  _alloc);
		_destroy_dissolved_links<Tcp_link> (_dissolved_tcp_links,  _alloc);
		_destroy_timed_out_arp_waiters();
		_destroy_released_dhcp_allocations(old_domain);

		/* do not consider to reuse IP config if the domains differ */
		if (old_domain.name() != new_domain_name) {
			return; }

		/* interface stays with its domain, so, try to reuse IP config */
		config.domains().with_element(
			new_domain_name,
			[&] /* match_fn */ (Domain &new_domain)
			{
				new_domain.try_reuse_ip_config(old_domain);
			},
			[&] /* no_match_fn */ () { }
		);
	});
}


void Interface::_failed_to_send_packet_link()
{
	if (_config_ptr->verbose()) {
		log("[", *_domain_ptr, "] failed to send packet (link down)"); }
}


void Interface::_failed_to_send_packet_submit()
{
	if (_config_ptr->verbose()) {
		log("[", *_domain_ptr, "] failed to send packet (queue full)"); }
}


void Interface::_failed_to_send_packet_alloc()
{
	if (_config_ptr->verbose()) {
		log("[", *_domain_ptr, "] failed to send packet (packet alloc failed)"); }
}


void Interface::handle_config_2()
{
	Domain_name const &new_domain_name = _policy.determine_domain_name();
	auto domain_fn = [&] (Domain &old_domain) {
		_config_ptr->domains().with_element(
			new_domain_name,
			[&] /* match_fn */ (Domain &new_domain)
			{
				/* if the domains differ, detach completely from the domain */
				if (old_domain.name() != new_domain_name) {

					_detach_from_domain();
					_attach_to_domain_raw(new_domain);

					/* destruct and construct DHCP client if required */
					if (old_domain.ip_config_dynamic()) {
						_dhcp_client.destruct();
					}
					if (new_domain.ip_config_dynamic()) {
						_dhcp_client.construct(_timer, *this);
					}
					return;
				}
				_update_domain_object(new_domain);

				/* destruct or construct DHCP client if IP-config type changes */
				if (old_domain.ip_config_dynamic() &&
				    !new_domain.ip_config_dynamic())
				{
					_dhcp_client.destruct();
				}
				if (!old_domain.ip_config_dynamic() &&
				    new_domain.ip_config_dynamic())
				{
					_dhcp_client.construct(_timer, *this);
				}

				/* remember that the interface stays attached to the same domain */
				_update_domain.construct(old_domain, new_domain);
			},
			[&] /* no_match_fn */ ()
			{
				/* the interface no longer has a domain */
				_detach_from_domain();

				/* destruct DHCP client if it was constructed */
				if (old_domain.ip_config_dynamic()) {
					_dhcp_client.destruct();
				}
			}
		);
	};
	auto no_domain_fn = [&] {
		/* the interface had no domain but now it may get one */
		_config_ptr->domains().with_element(
			new_domain_name,
			[&] /* match_fn */ (Domain &new_domain)
			{
				_attach_to_domain_raw(new_domain);

				/* construct DHCP client if the new domain needs it */
				if (new_domain.ip_config_dynamic()) {
					_dhcp_client.construct(_timer, *this);
				}
			},
			[&] /* no_match_fn */ () { }
		);
	};
	with_domain(domain_fn, no_domain_fn);
}


void Interface::handle_config_3()
{
	if (_update_domain.constructed()) {
		/*
		 * Update the domain object only if handle_config_2 determined that
		 * the interface stays attached to the same domain. Otherwise the
		 * interface already got detached from its old domain and there is
		 * nothing to update.
		 */
		Update_domain &update_domain = *_update_domain;
		Domain &old_domain = update_domain.old_domain;
		Domain &new_domain = update_domain.new_domain;
		_update_domain.destruct();

		/* if the IP configs differ, detach completely from the IP config */
		if (old_domain.ip_config() != new_domain.ip_config()) {
			detach_from_ip_config(old_domain);
			attach_to_domain_finish();
			return;
		}
		/* if there was/is no IP config, there is nothing more to update */
		if (!new_domain.ip_config().valid()) {
			return;
		}
		/* update state objects */
		_update_udp_tcp_links(L3_protocol::TCP, new_domain);
		_update_udp_tcp_links(L3_protocol::UDP, new_domain);
		_update_icmp_links(new_domain);
		_update_dhcp_allocations(old_domain, new_domain);
		_update_own_arp_waiters(new_domain);
	} else
		/* if the interface moved to another domain, finish the operation */
		with_domain([&] (Domain &) { attach_to_domain_finish(); });
}


void Interface::_ack_packet(Packet_descriptor const &pkt)
{
	if (!_sink.try_ack_packet(pkt)) {
		if (_config_ptr->verbose()) {
			log("[", *_domain_ptr, "] leak packet (sink not ready to "
			    "acknowledge)");
		}
		return;
	}
}


void Interface::cancel_arp_waiting(Arp_waiter &waiter)
{

	waiter.flush_packets(_alloc, [&] (Packet_descriptor const &packet) {
		_drop_packet(packet, "ARP got cancelled"); });
	destroy(_alloc, &waiter);
}


Interface::~Interface()
{
	_config_ptr->with_report([&] (Report &r) { r.handle_interface_link_state(); });
	_detach_from_domain();
	_interfaces.remove(this);
}



bool Interface::report_empty(Report const &report_cfg) const
{
	bool quota = report_cfg.quota() && !_policy.report_empty();
	bool stats = report_cfg.stats() && (
		!_tcp_stats.report_empty() || !_udp_stats.report_empty() || !_icmp_stats.report_empty() ||
		!_arp_stats.report_empty() || _dhcp_stats.report_empty());
	bool lnk_state = report_cfg.link_state();
	bool fragm_ip = report_cfg.dropped_fragm_ipv4() && _dropped_fragm_ipv4;
	return !quota && !lnk_state && !stats && !fragm_ip;
}


void Interface::report(Genode::Xml_generator &xml, Report const &report_cfg) const
{
	xml.attribute("label", _policy.label());
	if (report_cfg.link_state())
		xml.attribute("link_state", link_state());

	if (report_cfg.quota())
		_policy.report(xml);

	if (report_cfg.stats()) {
		if (!_tcp_stats.report_empty())  xml.node("tcp-links",        [&] { _tcp_stats.report(xml);  });
		if (!_udp_stats.report_empty())  xml.node("udp-links",        [&] { _udp_stats.report(xml);  });
		if (!_icmp_stats.report_empty()) xml.node("icmp-links",       [&] { _icmp_stats.report(xml); });
		if (!_arp_stats.report_empty())  xml.node("arp-waiters",      [&] { _arp_stats.report(xml);  });
		if (!_dhcp_stats.report_empty()) xml.node("dhcp-allocations", [&] { _dhcp_stats.report(xml); });
	}
	if (report_cfg.dropped_fragm_ipv4() && _dropped_fragm_ipv4)
		xml.node("dropped-fragm-ipv4", [&] {
			xml.attribute("value", _dropped_fragm_ipv4); });
}


/**************************
 ** Interface_link_stats **
 **************************/

Interface_link_stats::~Interface_link_stats()
{
	if (opening ||
	    open    ||
	    closing ||
	    closed)
	{
		error("closing interface has dangling links");
	}
}


/****************************
 ** Interface_object_stats **
 ****************************/

Interface_object_stats::~Interface_object_stats()
{
	if (alive) {
		error("closing interface has dangling links"); }
}
