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
static void _destroy_some_links(Link_list     &links,
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
	while (Link *link = links.first()) {
		_destroy_link<LINK_TYPE>(*link, links, dealloc);
		if (!--max) {
			return; }
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
	default: throw Interface::Bad_transport_protocol(); }
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
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _dst_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP:  return (*(Tcp_packet *)prot_base).dst_port();
	case L3_protocol::UDP:  return (*(Udp_packet *)prot_base).dst_port();
	case L3_protocol::ICMP: return Port((*(Icmp_packet *)prot_base).query_id());
	default: throw Interface::Bad_transport_protocol(); }
}


static void _dst_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP:  (*(Tcp_packet *)prot_base).dst_port(port);  return;
	case L3_protocol::UDP:  (*(Udp_packet *)prot_base).dst_port(port);  return;
	case L3_protocol::ICMP: (*(Icmp_packet *)prot_base).query_id(port.value); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _src_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP:  return (*(Tcp_packet *)prot_base).src_port();
	case L3_protocol::UDP:  return (*(Udp_packet *)prot_base).src_port();
	case L3_protocol::ICMP: return Port((*(Icmp_packet *)prot_base).query_id());
	default: throw Interface::Bad_transport_protocol(); }
}


static void _src_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP:  ((Tcp_packet *)prot_base)->src_port(port);        return;
	case L3_protocol::UDP:  ((Udp_packet *)prot_base)->src_port(port);        return;
	case L3_protocol::ICMP: ((Icmp_packet *)prot_base)->query_id(port.value); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void *_prot_base(L3_protocol const  prot,
                        Size_guard        &size_guard,
                        Ipv4_packet       &ip)
{
	switch (prot) {
	case L3_protocol::TCP:  return &ip.data<Tcp_packet>(size_guard);
	case L3_protocol::UDP:  return &ip.data<Udp_packet>(size_guard);
	case L3_protocol::ICMP: return &ip.data<Icmp_packet>(size_guard);
	default: throw Interface::Bad_transport_protocol(); }
}


/**************************
 ** Interface_link_stats **
 **************************/

void Interface_link_stats::report(Genode::Xml_generator &xml)
{
	bool empty = true;

	if (refused_for_ram)   { xml.node("refused_for_ram",   [&] () { xml.attribute("value", refused_for_ram); });   empty = false; }
	if (refused_for_ports) { xml.node("refused_for_ports", [&] () { xml.attribute("value", refused_for_ports); }); empty = false; }

	if (opening) { xml.node("opening", [&] () { xml.attribute("value", opening); }); empty = false; }
	if (open)    { xml.node("open",    [&] () { xml.attribute("value", open);    }); empty = false; }
	if (closing) { xml.node("closing", [&] () { xml.attribute("value", closing); }); empty = false; }
	if (closed)  { xml.node("closed",  [&] () { xml.attribute("value", closed);  }); empty = false; }

	if (dissolved_timeout_opening) { xml.node("dissolved_timeout_opening", [&] () { xml.attribute("value", dissolved_timeout_opening); }); empty = false; }
	if (dissolved_timeout_open)    { xml.node("dissolved_timeout_open",    [&] () { xml.attribute("value", dissolved_timeout_open);    }); empty = false; }
	if (dissolved_timeout_closing) { xml.node("dissolved_timeout_closing", [&] () { xml.attribute("value", dissolved_timeout_closing); }); empty = false; }
	if (dissolved_timeout_closed)  { xml.node("dissolved_timeout_closed",  [&] () { xml.attribute("value", dissolved_timeout_closed);  }); empty = false; }
	if (dissolved_no_timeout)      { xml.node("dissolved_no_timeout",      [&] () { xml.attribute("value", dissolved_no_timeout);      }); empty = false; }

	if (destroyed) { xml.node("destroyed", [&] () { xml.attribute("value", destroyed); }); empty = false; }

	if (empty) { throw Report::Empty(); }
}


/****************************
 ** Interface_object_stats **
 ****************************/

void Interface_object_stats::report(Genode::Xml_generator &xml)
{
	bool empty = true;

	if (alive)     { xml.node("alive",     [&] () { xml.attribute("value", alive);     }); empty = false; }
	if (destroyed) { xml.node("destroyed", [&] () { xml.attribute("value", destroyed); }); empty = false; }

	if (empty) { throw Report::Empty(); }
}


/***************
 ** Interface **
 ***************/

void Interface::_destroy_link(Link &link)
{
	L3_protocol const prot = link.protocol();
	switch (prot) {
	case L3_protocol::TCP:  ::_destroy_link<Tcp_link>(link, links(prot), _alloc);  break;
	case L3_protocol::UDP:  ::_destroy_link<Udp_link>(link, links(prot), _alloc);  break;
	case L3_protocol::ICMP: ::_destroy_link<Icmp_link>(link, links(prot), _alloc); break;
	default: throw Bad_transport_protocol(); }
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


Forward_rule_tree &
Interface::_forward_rules(Domain &local_domain, L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return local_domain.tcp_forward_rules();
	case L3_protocol::UDP: return local_domain.udp_forward_rules();
	default: throw Bad_transport_protocol(); }
}


Transport_rule_list &
Interface::_transport_rules(Domain &local_domain, L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return local_domain.tcp_rules();
	case L3_protocol::UDP: return local_domain.udp_rules();
	default: throw Bad_transport_protocol(); }
}


void Interface::_attach_to_domain_raw(Domain &domain)
{
	_domain = domain;
	_refetch_domain_ready_state();
	_interfaces.remove(this);
	domain.attach_interface(*this);
}


void Interface::_detach_from_domain_raw()
{
	Domain &domain = _domain();
	domain.detach_interface(*this);
	_interfaces.insert(this);
	_domain = Pointer<Domain>();
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
	Domain &old_domain = _domain();
	old_domain.interface_updates_domain_object(*this);
	_interfaces.insert(this);
	_domain = Pointer<Domain>();
	_refetch_domain_ready_state();

	old_domain.add_dropped_fragm_ipv4(_dropped_fragm_ipv4);
	old_domain.tcp_stats().dissolve_interface(_tcp_stats);
	old_domain.udp_stats().dissolve_interface(_udp_stats);
	old_domain.icmp_stats().dissolve_interface(_icmp_stats);
	old_domain.arp_stats().dissolve_interface(_arp_stats);
	old_domain.dhcp_stats().dissolve_interface(_dhcp_stats);

	/* attach raw */
	_domain = new_domain;
	_refetch_domain_ready_state();
	_interfaces.remove(this);
	new_domain.attach_interface(*this);
}


void Interface::attach_to_domain()
{
	_config().domains().with_element(
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
	Domain &domain = _domain();
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
	if (_domain.valid()) {
		handle_domain_ready_state(_domain().ready());
	} else {
		handle_domain_ready_state(false);
	}
}


void Interface::_reset_and_refetch_domain_ready_state()
{
	_policy.handle_domain_ready_state(false);
	_refetch_domain_ready_state();
}


void Interface::_detach_from_domain()
{
	try {
		detach_from_ip_config(domain());
		_detach_from_domain_raw();
	}
	catch (Pointer<Domain>::Invalid) { }
}


void
Interface::_new_link(L3_protocol             const  protocol,
                     Link_side_id            const &local,
                     Pointer<Port_allocator_guard>  remote_port_alloc,
                     Domain                        &remote_domain,
                     Link_side_id            const &remote)
{
	switch (protocol) {
	case L3_protocol::TCP:
		try {
			new (_alloc)
				Tcp_link { *this, local, remote_port_alloc, remote_domain,
				           remote, _timer, _config(), protocol, _tcp_stats };
		}
		catch (Out_of_ram)  { throw Free_resources_and_retry_handle_eth(L3_protocol::TCP); }
		catch (Out_of_caps) { throw Free_resources_and_retry_handle_eth(L3_protocol::TCP); }

		break;
	case L3_protocol::UDP:
		try {
			new (_alloc)
				Udp_link { *this, local, remote_port_alloc, remote_domain,
				           remote, _timer, _config(), protocol, _udp_stats };
		}
		catch (Out_of_ram)  { throw Free_resources_and_retry_handle_eth(L3_protocol::UDP); }
		catch (Out_of_caps) { throw Free_resources_and_retry_handle_eth(L3_protocol::UDP); }

		break;
	case L3_protocol::ICMP:
		try {
			new (_alloc)
				Icmp_link { *this, local, remote_port_alloc, remote_domain,
				            remote, _timer, _config(), protocol, _icmp_stats };
		}
		catch (Out_of_ram)  { throw Free_resources_and_retry_handle_eth(L3_protocol::ICMP); }
		catch (Out_of_caps) { throw Free_resources_and_retry_handle_eth(L3_protocol::ICMP); }

		break;
	default: throw Bad_transport_protocol(); }
}


void Interface::dhcp_allocation_expired(Dhcp_allocation &allocation)
{
	_release_dhcp_allocation(allocation, _domain());
	_released_dhcp_allocations.insert(&allocation);
}


Link_list &Interface::links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _tcp_links;
	case L3_protocol::UDP:  return _udp_links;
	case L3_protocol::ICMP: return _icmp_links;
	default: throw Bad_transport_protocol(); }
}


Link_list &Interface::dissolved_links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _dissolved_tcp_links;
	case L3_protocol::UDP:  return _dissolved_udp_links;
	case L3_protocol::ICMP: return _dissolved_icmp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::_adapt_eth(Ethernet_frame          &eth,
                           Ipv4_address      const &dst_ip,
                           Packet_descriptor const &pkt,
                           Domain                  &remote_domain)
{
	Ipv4_config const &remote_ip_cfg = remote_domain.ip_config();
	if (!remote_ip_cfg.valid()) {
		throw Drop_packet("target domain has yet no IP config");
	}
	if (remote_domain.use_arp()) {

		Ipv4_address const &hop_ip = remote_domain.next_hop(dst_ip);
		remote_domain.arp_cache().find_by_ip(
			hop_ip,
			[&] /* handle_match */ (Arp_cache_entry const &entry)
			{
				eth.dst(entry.mac());
			},
			[&] /* handle_no_match */ ()
			{
				remote_domain.interfaces().for_each([&] (Interface &interface)
				{
					interface._broadcast_arp_request(
						remote_ip_cfg.interface().address, hop_ip);
				});
				try { new (_alloc) Arp_waiter { *this, remote_domain, hop_ip, pkt }; }
				catch (Out_of_ram)  { throw Free_resources_and_retry_handle_eth(); }
				catch (Out_of_caps) { throw Free_resources_and_retry_handle_eth(); }
				throw Packet_postponed();
			}
		);
	}
}


void Interface::_nat_link_and_pass(Ethernet_frame         &eth,
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
	try {
		Pointer<Port_allocator_guard> remote_port_alloc;
		remote_domain.nat_rules().find_by_domain(
			local_domain,
			[&] /* handle_match */ (Nat_rule &nat)
			{
				if(_config().verbose()) {
					log("[", local_domain, "] using NAT rule: ", nat); }

				_src_port(prot, prot_base, nat.port_alloc(prot).alloc());
				ip.src(remote_domain.ip_config().interface().address, ip_icd);
				remote_port_alloc = nat.port_alloc(prot);
			},
			[&] /* no_match */ () { }
		);
		Link_side_id const remote_id = { ip.dst(), _dst_port(prot, prot_base),
		                                 ip.src(), _src_port(prot, prot_base) };
		_new_link(prot, local_id, remote_port_alloc, remote_domain, remote_id);
		_pass_prot_to_domain(
			remote_domain, eth, size_guard, ip, ip_icd, prot, prot_base,
			prot_size);

	} catch (Port_allocator_guard::Out_of_indices) {
		switch (prot) {
		case L3_protocol::TCP:  _tcp_stats.refused_for_ports++;  break;
		case L3_protocol::UDP:  _udp_stats.refused_for_ports++;  break;
		case L3_protocol::ICMP: _icmp_stats.refused_for_ports++; break;
		default: throw Bad_transport_protocol(); }
	}
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
	if (_config().verbose()) {
		log("[", local_domain, "] release DHCP allocation: ", allocation); }

	_dhcp_allocations.remove(allocation);
}


void Interface::_new_dhcp_allocation(Ethernet_frame &eth,
                                     Dhcp_packet    &dhcp,
                                     Dhcp_server    &dhcp_srv,
                                     Domain         &local_domain)
{
	try {
		Dhcp_allocation &allocation = *new (_alloc)
			Dhcp_allocation { *this, dhcp_srv.alloc_ip(), dhcp.client_mac(),
			                  _timer, _config().dhcp_offer_timeout() };

		_dhcp_allocations.insert(allocation);
		if (_config().verbose()) {
			log("[", local_domain, "] offer DHCP allocation: ", allocation); }

		_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
		                 allocation.ip(),
		                 Dhcp_packet::Message_type::OFFER,
		                 dhcp.xid(),
		                 local_domain.ip_config().interface());
	}
	catch (Out_of_ram)  { throw Free_resources_and_retry_handle_eth(); }
	catch (Out_of_caps) { throw Free_resources_and_retry_handle_eth(); }
}


void Interface::_handle_dhcp_request(Ethernet_frame            &eth,
                                     Dhcp_packet               &dhcp,
                                     Domain                    &local_domain,
                                     Ipv4_address_prefix const &local_intf)
{
	try {
		/* try to get the DHCP server config of this interface */
		Dhcp_server &dhcp_srv = local_domain.dhcp_server();

		/* determine type of DHCP request */
		Dhcp_packet::Message_type const msg_type =
			dhcp.option<Dhcp_packet::Message_type_option>().value();

		try {
			/* look up existing DHCP configuration for client */
			Dhcp_allocation &allocation =
				_dhcp_allocations.find_by_mac(dhcp.client_mac());

			switch (msg_type) {
			case Dhcp_packet::Message_type::DISCOVER:

				if (allocation.bound()) {

					_release_dhcp_allocation(allocation, local_domain);
					_destroy_dhcp_allocation(allocation, local_domain);
					_new_dhcp_allocation(eth, dhcp, dhcp_srv, local_domain);
					return;

				} else {
					allocation.lifetime(_config().dhcp_offer_timeout());
					_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid(), local_intf);
					return;
				}
			case Dhcp_packet::Message_type::REQUEST:

				if (allocation.bound()) {
					allocation.lifetime(dhcp_srv.ip_lease_time());
					_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::ACK,
					                 dhcp.xid(), local_intf);
					return;

				} else {
					Dhcp_packet::Server_ipv4 &dhcp_srv_ip =
						dhcp.option<Dhcp_packet::Server_ipv4>();

					if (dhcp_srv_ip.value() == local_intf.address)
					{
						allocation.set_bound();
						allocation.lifetime(dhcp_srv.ip_lease_time());
						if (_config().verbose()) {
							log("[", local_domain, "] bind DHCP allocation: ",
							    allocation);
						}
						_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
						                 allocation.ip(),
						                 Dhcp_packet::Message_type::ACK,
						                 dhcp.xid(), local_intf);
						return;

					} else {

						_release_dhcp_allocation(allocation, local_domain);
						_destroy_dhcp_allocation(allocation, local_domain);
						return;
					}
				}
			case Dhcp_packet::Message_type::INFORM:

				_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
				                 allocation.ip(),
				                 Dhcp_packet::Message_type::ACK,
				                 dhcp.xid(), local_intf);
				return;

			case Dhcp_packet::Message_type::DECLINE:
			case Dhcp_packet::Message_type::RELEASE:

				_release_dhcp_allocation(allocation, local_domain);
				_destroy_dhcp_allocation(allocation, local_domain);
				return;

			case Dhcp_packet::Message_type::NAK:   throw Drop_packet("DHCP NAK from client");
			case Dhcp_packet::Message_type::OFFER: throw Drop_packet("DHCP OFFER from client");
			case Dhcp_packet::Message_type::ACK:   throw Drop_packet("DHCP ACK from client");
			default:                               throw Drop_packet("DHCP request with broken message type");
			}
		}
		catch (Dhcp_allocation_tree::No_match) {

			switch (msg_type) {
			case Dhcp_packet::Message_type::DISCOVER:

				_new_dhcp_allocation(eth, dhcp, dhcp_srv, local_domain);
				return;

			case Dhcp_packet::Message_type::REQUEST:

				_send_dhcp_reply(dhcp_srv, eth.src(), dhcp.client_mac(),
				                 Ipv4_address { },
				                 Dhcp_packet::Message_type::NAK,
				                 dhcp.xid(), local_intf);
				return;

			case Dhcp_packet::Message_type::DECLINE: throw Drop_packet("DHCP DECLINE from client without offered/acked IP");
			case Dhcp_packet::Message_type::RELEASE: throw Drop_packet("DHCP RELEASE from client without offered/acked IP");
			case Dhcp_packet::Message_type::NAK:     throw Drop_packet("DHCP NAK from client");
			case Dhcp_packet::Message_type::OFFER:   throw Drop_packet("DHCP OFFER from client");
			case Dhcp_packet::Message_type::ACK:     throw Drop_packet("DHCP ACK from client");
			default:                                 throw Drop_packet("DHCP request with broken message type");
			}
		}
	}
	catch (Dhcp_packet::Option_not_found exception) {
		throw Drop_packet("DHCP request misses required option"); }
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
	struct Keep_ip_config : Exception { };
	try {
		attach_to_domain_finish();

		/* if the whole domain is down, discard IP config */
		Domain &domain_ = domain();
		if (!link_state() && domain_.ip_config().valid()) {
			domain_.interfaces().for_each([&] (Interface &interface) {
				if (interface.link_state()) {
					throw Keep_ip_config(); }
			});
			domain_.discard_ip_config();
			domain_.arp_cache().destroy_all_entries();
		}
	}
	catch (Pointer<Domain>::Invalid) { }
	catch (Domain::Ip_config_static) { }
	catch (Keep_ip_config) { }

	/* force report if configured */
	try { _config().report().handle_interface_link_state(); }
	catch (Pointer<Report>::Invalid) { }
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


void Interface::_handle_icmp_query(Ethernet_frame          &eth,
                                   Size_guard              &size_guard,
                                   Ipv4_packet             &ip,
                                   Internet_checksum_diff  &ip_icd,
                                   Packet_descriptor const &pkt,
                                   L3_protocol              prot,
                                   void                    *prot_base,
                                   size_t                   prot_size,
                                   Domain                  &local_domain)
{
	Link_side_id const local_id = { ip.src(), _src_port(prot, prot_base),
	                                ip.dst(), _dst_port(prot, prot_base) };

	/* try to route via existing ICMP links */
	bool done { false };
	local_domain.links(prot).find_by_id(
		local_id,
		[&] /* handle_match */ (Link_side const &local_side)
		{
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Domain &remote_domain = remote_side.domain();
			if (_config().verbose()) {
				log("[", local_domain, "] using ", l3_protocol_name(prot),
				    " link: ", link);
			}
			_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
			ip.src(remote_side.dst_ip(), ip_icd);
			ip.dst(remote_side.src_ip(), ip_icd);
			_src_port(prot, prot_base, remote_side.dst_port());
			_dst_port(prot, prot_base, remote_side.src_port());
			_pass_prot_to_domain(
				remote_domain, eth, size_guard, ip, ip_icd, prot,
				prot_base, prot_size);

			_link_packet(prot, prot_base, link, client);
			done = true;
		},
		[&] /* handle_no_match */ () { }
	);
	if (done) {
		return;
	}

	/* try to route via ICMP rules */
	local_domain.icmp_rules().find_longest_prefix_match(
		ip.dst(),
		[&] /* handle_match */ (Ip_rule const &rule)
		{
			if(_config().verbose()) {
				log("[", local_domain, "] using ICMP rule: ", rule); }

			Domain &remote_domain = rule.domain();
			_adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
			_nat_link_and_pass(eth, size_guard, ip, ip_icd, prot, prot_base,
			                   prot_size, local_id, local_domain,
			                   remote_domain);

			done = true;
		},
		[&] /* handle_no_match */ () { }
	);
	if (done) {
		return;
	}

	throw Bad_transport_protocol();
}


void Interface::_handle_icmp_error(Ethernet_frame          &eth,
                                   Size_guard              &size_guard,
                                   Ipv4_packet             &ip,
                                   Internet_checksum_diff  &ip_icd,
                                   Packet_descriptor const &pkt,
                                   Domain                  &local_domain,
                                   Icmp_packet             &icmp,
                                   size_t                   icmp_sz)
{
	Ipv4_packet            &embed_ip     { icmp.data<Ipv4_packet>(size_guard) };
	Internet_checksum_diff  embed_ip_icd { };

	/* drop packet if embedded IP checksum invalid */
	if (embed_ip.checksum_error()) {
		throw Drop_packet("bad checksum in IP packet embedded in ICMP error");
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
			if (_config().verbose()) {
				log("[", local_domain, "] using ",
				    l3_protocol_name(embed_prot), " link: ", link);
			}
			/* adapt source and destination of Ethernet frame and IP packet */
			_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
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
		},
		[&] /* handle_no_match */ ()
		{
			throw Drop_packet("no link that matches packet embedded in "
			                  "ICMP error");
		}
	);
}


void Interface::_handle_icmp(Ethernet_frame            &eth,
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
	Icmp_packet &icmp = *reinterpret_cast<Icmp_packet *>(prot_base);
	if (icmp.checksum_error(size_guard.unconsumed())) {
		throw Drop_packet("bad ICMP checksum"); }

	/* try to act as ICMP Echo server */
	if (icmp.type() == Icmp_packet::Type::ECHO_REQUEST &&
	    ip.dst() == local_intf.address &&
	    local_domain.icmp_echo_server())
	{
		if(_config().verbose()) {
			log("[", local_domain, "] act as ICMP Echo server"); }

		_send_icmp_echo_reply(eth, ip, icmp, prot_size, size_guard);
		return;
	}
	/* try to act as ICMP router */
	switch (icmp.type()) {
	case Icmp_packet::Type::ECHO_REPLY:
	case Icmp_packet::Type::ECHO_REQUEST:    _handle_icmp_query(eth, size_guard, ip, ip_icd, pkt, prot, prot_base, prot_size, local_domain); break;
	case Icmp_packet::Type::DST_UNREACHABLE: _handle_icmp_error(eth, size_guard, ip, ip_icd, pkt, local_domain, icmp, prot_size); break;
	default: Drop_packet("unhandled type in ICMP"); }
}


void Interface::_handle_ip(Ethernet_frame          &eth,
                           Size_guard              &size_guard,
                           Packet_descriptor const &pkt,
                           Domain                  &local_domain)
{
	Ipv4_packet            &ip     { eth.data<Ipv4_packet>(size_guard) };
	Internet_checksum_diff  ip_icd { };

	/* drop fragmented IPv4 as it isn't supported */
	Ipv4_address_prefix const &local_intf = local_domain.ip_config().interface();
	if (ip.more_fragments() ||
	    ip.fragment_offset() != 0) {

		_dropped_fragm_ipv4++;
		if (_config().icmp_type_3_code_on_fragm_ipv4() != Icmp_packet::Code::INVALID) {
			_send_icmp_dst_unreachable(
				local_intf, eth, ip, _config().icmp_type_3_code_on_fragm_ipv4());
		}
		throw Drop_packet("fragmented IPv4 not supported");
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
		return;
	}

	/* try to route via transport layer rules */
	bool done { false };
	try {
		L3_protocol  const prot      = ip.protocol();
		size_t       const prot_size = size_guard.unconsumed();
		void        *const prot_base = _prot_base(prot, size_guard, ip);

		/* try handling DHCP requests before trying any routing */
		if (prot == L3_protocol::UDP) {
			Udp_packet &udp = *reinterpret_cast<Udp_packet *>(prot_base);

			if (Dhcp_packet::is_dhcp(&udp)) {

				/* get DHCP packet */
				Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
				switch (dhcp.op()) {
				case Dhcp_packet::REQUEST:

					try {
						_handle_dhcp_request(
							eth, dhcp, local_domain, local_intf);
					}
					catch (Pointer<Dhcp_server>::Invalid) {
						throw Drop_packet("DHCP request while DHCP server inactive");
					}
					return;

				case Dhcp_packet::REPLY:

					if (eth.dst() != router_mac() &&
					    eth.dst() != Mac_address(0xff))
					{
						throw Drop_packet("Ethernet of DHCP reply doesn't target router"); }

					if (dhcp.client_mac() != router_mac()) {
						throw Drop_packet("DHCP reply doesn't target router"); }

					if (!_dhcp_client.constructed()) {
						throw Drop_packet("DHCP reply while DHCP client inactive"); }

					_dhcp_client->handle_dhcp_reply(dhcp);
					return;

				default:

					throw Drop_packet("Bad DHCP opcode");
				}
			}
		}
		else if (prot == L3_protocol::ICMP) {
			_handle_icmp(eth, size_guard, ip, ip_icd, pkt, prot, prot_base,
			             prot_size, local_domain, local_intf);
			return;
		}

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
				if (_config().verbose()) {
					log("[", local_domain, "] using ", l3_protocol_name(prot),
					    " link: ", link);
				}
				_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
				ip.src(remote_side.dst_ip(), ip_icd);
				ip.dst(remote_side.src_ip(), ip_icd);
				_src_port(prot, prot_base, remote_side.dst_port());
				_dst_port(prot, prot_base, remote_side.src_port());
				_pass_prot_to_domain(
					remote_domain, eth, size_guard, ip, ip_icd, prot,
					prot_base, prot_size);

				_link_packet(prot, prot_base, link, client);
				done = true;
			},
			[&] /* handle_no_match */ () { }
		);
		if (done) {
			return;
		}

		/* try to route via forward rules */
		if (local_id.dst_ip == local_intf.address) {

			_forward_rules(local_domain, prot).find_by_port(
				local_id.dst_port,
				[&] /* handle_match */ (Forward_rule const &rule)
				{
					if(_config().verbose()) {
						log("[", local_domain, "] using forward rule: ",
						    l3_protocol_name(prot), " ", rule);
					}
					Domain &remote_domain = rule.domain();
					_adapt_eth(eth, rule.to_ip(), pkt, remote_domain);
					ip.dst(rule.to_ip(), ip_icd);
					if (!(rule.to_port() == Port(0))) {
						_dst_port(prot, prot_base, rule.to_port());
					}
					_nat_link_and_pass(
						eth, size_guard, ip, ip_icd, prot, prot_base,
						prot_size, local_id, local_domain, remote_domain);

					done = true;
				},
				[&] /* handle_no_match */ () { }
			);
			if (done) {
				return;
			}
		}
		/* try to route via transport and permit rules */
		_transport_rules(local_domain, prot).find_best_match(
			local_id.dst_ip,
			local_id.dst_port,
			[&] /* handle_match */ (Transport_rule const &transport_rule,
			                        Permit_rule    const &permit_rule)
			{
				if(_config().verbose()) {
					log("[", local_domain, "] using ",
					    l3_protocol_name(prot), " rule: ",
					    transport_rule, " ", permit_rule);
				}
				Domain &remote_domain = permit_rule.domain();
				_adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
				_nat_link_and_pass(
					eth, size_guard, ip, ip_icd, prot, prot_base, prot_size,
					local_id, local_domain, remote_domain);

				done = true;
			},
			[&] /* handle_no_match */ () { }
		);
		if (done) {
			return;
		}
	}
	catch (Interface::Bad_transport_protocol) { }

	/* try to route via IP rules */
	local_domain.ip_rules().find_longest_prefix_match(
		ip.dst(),
		[&] /* handle_match */ (Ip_rule const &rule)
		{
			if(_config().verbose()) {
				log("[", local_domain, "] using IP rule: ", rule); }

			Domain &remote_domain = rule.domain();
			_adapt_eth(eth, ip.dst(), pkt, remote_domain);
			remote_domain.interfaces().for_each([&] (Interface &interface) {
				interface.send(eth, size_guard);
			});
			done = true;
		},
		[&] /* handle_no_match */ () { }
	);
	if (done) {
		return;
	}

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
		                           Icmp_packet::Code::DST_NET_UNREACHABLE);
	}
	if (_config().verbose()) {
		log("[", local_domain, "] unroutable packet"); }
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
			if (_config().verbose()) {
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
				waiter.src()._continue_handle_eth(local_domain, waiter.packet());
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
		if (_config().verbose()) {
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


void Interface::_handle_arp_request(Ethernet_frame &eth,
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
			throw Drop_packet("gratuitous ARP request");

		} else if (arp.dst_ip() == local_intf.address) {

			/* ARP request for the routers IP at this domain */
			if (_config().verbose()) {
				log("[", local_domain, "] answer ARP request for router IP "
				    "with router MAC"); }
			_send_arp_reply(eth, arp);

		} else {

			/* forward request to all other interfaces of the domain */
			if (_config().verbose()) {
				log("[", local_domain, "] forward ARP request for local IP "
				    "to all interfaces of the sender domain"); }
			_domain_broadcast(eth, size_guard, local_domain);
		}

	} else {

		/* ARP request for an IP foreign to the domain's subnet */
		if (local_ip_cfg.gateway_valid()) {

			/* leave request up to the gateway of the domain */
			throw Drop_packet("leave ARP request up to gateway");

		} else {

			/* try to act as gateway for the domain as none is configured */
			if (_config().verbose()) {
				log("[", local_domain, "] answer ARP request for foreign IP "
				    "with router MAC"); }
			_send_arp_reply(eth, arp);
		}
	}
}


void Interface::_handle_arp(Ethernet_frame &eth,
                            Size_guard     &size_guard,
                            Domain         &local_domain)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	Arp_packet &arp = eth.data<Arp_packet>(size_guard);
	if (!arp.ethernet_ipv4()) {
		throw Drop_packet("ARP for unknown protocol"); }

	switch (arp.opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(eth, size_guard, arp, local_domain);   break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, size_guard, arp, local_domain); break;
	default: throw Drop_packet("unknown ARP operation"); }
}


void Interface::_handle_pkt()
{
	Packet_descriptor const pkt = _sink.get_packet();
	Size_guard size_guard(pkt.size());
	try {
		_handle_eth(_sink.packet_content(pkt), size_guard, pkt);
		_ack_packet(pkt);
	}
	catch (Packet_postponed) { }
	catch (Genode::Packet_descriptor::Invalid_packet) { }
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
	unsigned long const max_pkts = _config().max_packets_per_signal();
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
	_config().domains().for_each([&] (Domain &domain) {
		domain.interfaces().for_each([&] (Interface &interface) {
			interface.wakeup_source();
		});
	});
	wakeup_sink();
}


void Interface::_continue_handle_eth(Domain            const &domain,
                                     Packet_descriptor const &pkt)
{
	Size_guard size_guard(pkt.size());
	try { _handle_eth(_sink.packet_content(pkt), size_guard, pkt); }
	catch (Packet_postponed) {
		if (domain.verbose_packet_drop()) {
			log("[", domain, "] drop packet (handling postponed twice)"); }
	}
	catch (Genode::Packet_descriptor::Invalid_packet) {
		if (domain.verbose_packet_drop()) {
			log("[", domain, "] invalid Nic packet received");
		}
	}
	_ack_packet(pkt);
}


void Interface::_destroy_dhcp_allocation(Dhcp_allocation &allocation,
                                         Domain          &local_domain)
{
	try { local_domain.dhcp_server().free_ip(local_domain, allocation.ip()); }
	catch (Pointer<Dhcp_server>::Invalid) { }
	destroy(_alloc, &allocation);
}


void Interface::_destroy_released_dhcp_allocations(Domain &local_domain)
{
	while (Dhcp_allocation *allocation = _released_dhcp_allocations.first()) {
		_released_dhcp_allocations.remove(allocation);
		_destroy_dhcp_allocation(*allocation, local_domain);
	}
}


void Interface::_handle_eth(Ethernet_frame           &eth,
                            Size_guard               &size_guard,
                            Packet_descriptor  const &pkt,
                            Domain                   &local_domain)
{
	if (local_domain.ip_config().valid()) {

		switch (eth.type()) {
		case Ethernet_frame::Type::ARP:  _handle_arp(eth, size_guard, local_domain);     break;
		case Ethernet_frame::Type::IPV4: _handle_ip(eth, size_guard, pkt, local_domain); break;
		default: throw Bad_network_protocol(); }

	} else {

		switch (eth.type()) {
		case Ethernet_frame::Type::IPV4: {

			if (eth.dst() != router_mac() &&
			    eth.dst() != Mac_address(0xff))
			{
				throw Drop_packet("Expecting Ethernet targeting the router"); }

			Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
			if (ip.protocol() != Ipv4_packet::Protocol::UDP) {
				throw Drop_packet("Expecting UDP packet"); }

			Udp_packet &udp = ip.data<Udp_packet>(size_guard);
			if (!Dhcp_packet::is_dhcp(&udp)) {
				throw Drop_packet("Expecting DHCP packet"); }

			Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
			switch (dhcp.op()) {
			case Dhcp_packet::REPLY:

				if (dhcp.client_mac() != router_mac()) {
					throw Drop_packet("Expecting DHCP targeting the router"); }

				if (!_dhcp_client.constructed()) {
					throw Drop_packet("Expecting DHCP client to be active"); }

				_dhcp_client->handle_dhcp_reply(dhcp);
				break;

			default:

				throw Drop_packet("Expecting DHCP reply");
			}
			break;
		}
		case Ethernet_frame::Type::ARP: {
				throw Drop_packet("Ignore ARP request on unconfigured interface");
		}
		default:

			throw Bad_network_protocol();
		}
	}
}


void Interface::_handle_eth(void              *const  eth_base,
                            Size_guard               &size_guard,
                            Packet_descriptor  const &pkt)
{
	try {
		Domain &local_domain = _domain();
		local_domain.raise_rx_bytes(size_guard.total_size());
		try {
			Ethernet_frame &eth = Ethernet_frame::cast_from(eth_base, size_guard);
			try {
				/* do garbage collection over transport-layer links and DHCP allocations */
				_destroy_dissolved_links<Icmp_link>(_dissolved_icmp_links, _alloc);
				_destroy_dissolved_links<Udp_link>(_dissolved_udp_links,   _alloc);
				_destroy_dissolved_links<Tcp_link>(_dissolved_tcp_links,   _alloc);
				_destroy_released_dhcp_allocations(local_domain);

				/* log received packet if desired */
				if (local_domain.verbose_packets()) {
					log("[", local_domain, "] rcv ", eth); }

				if (local_domain.trace_packets())
					Genode::Trace::Ethernet_packet(local_domain.name().string(),
					                               Genode::Trace::Ethernet_packet::Direction::RECV,
					                               eth_base,
					                               size_guard.total_size());

				/* try to handle ethernet frame */
				try { _handle_eth(eth, size_guard, pkt, local_domain); }
				catch (Free_resources_and_retry_handle_eth) {
					try {
						if (_config().verbose()) {
							log("[", local_domain, "] free resources and retry to handle packet"); }

						/*
						 * Resources do not suffice, destroy some links
						 *
						 * Limit number of links to destroy because otherwise,
						 * this could block the router for a significant
						 * amount of time.
						 */
						unsigned long max = MAX_FREE_OPS_PER_EMERGENCY;
						_destroy_some_links<Tcp_link> (_tcp_links,  _dissolved_tcp_links,  _alloc, max);
						_destroy_some_links<Udp_link> (_udp_links,  _dissolved_udp_links,  _alloc, max);
						_destroy_some_links<Icmp_link>(_icmp_links, _dissolved_icmp_links, _alloc, max);

						/* retry to handle ethernet frame */
						_handle_eth(eth, size_guard, pkt, local_domain);
					}
					catch (Free_resources_and_retry_handle_eth exception) {
						if (exception.prot != (L3_protocol)0) {
							switch (exception.prot) {
							case L3_protocol::TCP:  _tcp_stats.refused_for_ram++;  break;
							case L3_protocol::UDP:  _udp_stats.refused_for_ram++;  break;
							case L3_protocol::ICMP: _icmp_stats.refused_for_ram++; break;
							default: throw Bad_transport_protocol(); }
						}

						/* give up if the resources still not suffice */
						throw Drop_packet("insufficient resources");
					}
				}
			}
			catch (Dhcp_server::Alloc_ip_failed) {
				if (_config().verbose()) {
					log("[", local_domain, "] failed to allocate IP for DHCP "
					    "client");
				}
			}
			catch (Port_allocator_guard::Out_of_indices) {
				if (_config().verbose()) {
					log("[", local_domain, "] no available NAT ports"); }
			}
			catch (Domain::No_next_hop) {
				if (_config().verbose()) {
					log("[", local_domain, "] cannot find next hop"); }
			}
			catch (Alloc_dhcp_msg_buffer_failed) {
				if (_config().verbose()) {
					log("[", local_domain, "] failed to allocate buffer for "
					    "DHCP reply");
				}
			}
			catch (Bad_network_protocol) {
				if (_config().verbose()) {
					log("[", local_domain, "] unknown network layer "
					    "protocol");
				}
			}
			catch (Drop_packet exception) {
				if (local_domain.verbose_packet_drop()) {
					log("[", local_domain, "] drop packet (",
					    exception.reason, ")");
				}
			}
		}
		catch (Size_guard::Exceeded) {
			if (_config().verbose()) {
				log("[", local_domain, "] drop packet: packet size-guard "
				    "exceeded");
			}
		}
	}
	catch (Pointer<Domain>::Invalid) {
		try {
			Ethernet_frame &eth = Ethernet_frame::cast_from(eth_base, size_guard);
			if (_config().verbose_packets()) {
				log("[?] rcv ", eth); }
		}
		catch (Size_guard::Exceeded) {
			if (_config().verbose_packets()) {
				log("[?] rcv ?"); }
		}
		if (_config().verbose()) {
			log("[?] drop packet: no domain"); }
	}
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
	Domain &local_domain = _domain();
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
	_config                    { config },
	_policy                    { policy },
	_timer                     { timer },
	_alloc                     { alloc },
	_interfaces                { interfaces }
{
	_interfaces.insert(this);
	try { _config().report().handle_interface_link_state(); }
	catch (Pointer<Report>::Invalid) { }
}


void Interface::_dismiss_link_log(Link       &link,
                                  char const *reason)
{
	if (!_config().verbose()) {
		return;
	}
	log("[", link.client().domain(), "] dismiss link client: ", link.client(), " (", reason, ")");
	log("[", link.server().domain(), "] dismiss link server: ", link.server(), " (", reason, ")");
}


void Interface::_update_link_check_nat(Link        &link,
                                       Domain      &new_srv_dom,
                                       L3_protocol  prot,
                                       Domain      &cln_dom)
{
	/* if server domain or its IP config changed, dismiss link */
	Domain &old_srv_dom = link.server().domain();
	if (old_srv_dom.name()      != new_srv_dom.name() ||
	    old_srv_dom.ip_config() != new_srv_dom.ip_config())
	{
		_dismiss_link_log(link, "rule targets other domain");
		throw Dismiss_link();
	}
	Pointer<Port_allocator_guard> remote_port_alloc_ptr;
	if (link.client().src_ip() == link.server().dst_ip()) {
		link.handle_config(cln_dom, new_srv_dom, remote_port_alloc_ptr, _config());
		return;
	}
	try {
		if (link.server().dst_ip() != new_srv_dom.ip_config().interface().address) {
			_dismiss_link_log(link, "NAT IP");
			throw Dismiss_link();
		}
		bool done { false };
		new_srv_dom.nat_rules().find_by_domain(
			cln_dom,
			[&] /* handle_match */ (Nat_rule &nat)
			{
				Port_allocator_guard &remote_port_alloc = nat.port_alloc(prot);
				remote_port_alloc.alloc(link.server().dst_port());
				remote_port_alloc_ptr = remote_port_alloc;
				link.handle_config(
					cln_dom, new_srv_dom, remote_port_alloc_ptr, _config());

				done = true;
			},
			[&] /* handle_no_match */ ()
			{
				_dismiss_link_log(link, "no NAT rule");
			}
		);
		if (done) {
			return;
		}
	}
	catch (Port_allocator::Allocation_conflict)  { _dismiss_link_log(link, "no NAT-port"); }
	catch (Port_allocator_guard::Out_of_indices) { _dismiss_link_log(link, "no NAT-port quota"); }
	throw Dismiss_link();
}


void Interface::_update_udp_tcp_links(L3_protocol  prot,
                                      Domain      &cln_dom)
{
	links(prot).for_each([&] (Link &link) {

		try {
			/* try to find forward rule that matches the server port */
			_forward_rules(cln_dom, prot).find_by_port(
				link.client().dst_port(),
				[&] /* handle_match */ (Forward_rule const &rule)
				{
					/* if destination IP of forwarding changed, dismiss link */
					if (rule.to_ip() != link.server().src_ip()) {
						_dismiss_link_log(link, "other forward-rule to");
						throw Dismiss_link();
					}
					/*
					 * If destination port of forwarding was set and then was
					 * modified or unset, dismiss link
					 */
					if (!(link.server().src_port() == link.client().dst_port())) {
						if (!(rule.to_port() == link.server().src_port())) {
							_dismiss_link_log(link, "other forward-rule to_port");
							throw Dismiss_link();
						}
					}
					/*
					 * If destination port of forwarding was not set and then was
					 * set, dismiss link
					 */
					else {
						if (!(rule.to_port() == link.server().src_port()) &&
						    !(rule.to_port() == Port(0)))
						{
							_dismiss_link_log(link, "new forward-rule to_port");
							throw Dismiss_link();
						}
					}
					_update_link_check_nat(link, rule.domain(), prot, cln_dom);
					return;
				},
				[&] /* handle_no_match */ () {
					try {
						/* try to find transport rule that matches the server IP */
						bool done { false };
						_transport_rules(cln_dom, prot).find_best_match(
							link.client().dst_ip(),
							link.client().dst_port(),
							[&] /* handle_match */ (Transport_rule const &,
							                        Permit_rule    const &permit_rule)
							{
								_update_link_check_nat(link, permit_rule.domain(), prot, cln_dom);
								done = true;
							},
							[&] /* handle_no_match */ ()
							{
								_dismiss_link_log(link, "no matching transport/permit/forward rule");
							}
						);
						if (done) {
							return;
						}
					}
					catch (Dismiss_link) { }
				}
			);
		}
		catch (Dismiss_link) { }
		_destroy_link(link);
	});
}


void Interface::_update_icmp_links(Domain &cln_dom)
{
	L3_protocol const prot = L3_protocol::ICMP;
	links(prot).for_each([&] (Link &link) {
		try {
			bool done { false };
			cln_dom.icmp_rules().find_longest_prefix_match(
				link.client().dst_ip(),
				[&] /* handle_match */ (Ip_rule const &rule)
				{
					_update_link_check_nat(link, rule.domain(), prot, cln_dom);
					done = true;
				},
				[&] /* handle_no_match */ ()
				{
					_dismiss_link_log(link, "no ICMP rule");
				}
			);
			if (done) {
				return;
			}
		}
		catch (Dismiss_link) { }
		_destroy_link(link);
	});
}


void Interface::_update_dhcp_allocations(Domain &old_domain,
                                         Domain &new_domain)
{
	bool dhcp_clients_outdated { false };
	try {
		Dhcp_server &old_dhcp_srv = old_domain.dhcp_server();
		Dhcp_server &new_dhcp_srv = new_domain.dhcp_server();
		if (!old_dhcp_srv.config_equal_to_that_of(new_dhcp_srv)) {
			throw Pointer<Dhcp_server>::Invalid();
		}
		_dhcp_allocations.for_each([&] (Dhcp_allocation &allocation) {
			try {
				new_dhcp_srv.alloc_ip(allocation.ip());

				/* keep DHCP allocation */
				if (_config().verbose()) {
					log("[", new_domain, "] update DHCP allocation: ",
					    allocation);
				}
				return;
			}
			catch (Dhcp_server::Alloc_ip_failed) {
				if (_config().verbose()) {
					log("[", new_domain, "] dismiss DHCP allocation: ",
					    allocation, " (no IP)");
				}
			}
			/* dismiss DHCP allocation */
			dhcp_clients_outdated = true;
			_dhcp_allocations.remove(allocation);
			_destroy_dhcp_allocation(allocation, old_domain);
		});
	}
	catch (Pointer<Dhcp_server>::Invalid) {

		/* dismiss all DHCP allocations */
		dhcp_clients_outdated = true;
		while (Dhcp_allocation *allocation = _dhcp_allocations.first()) {
			if (_config().verbose()) {
				log("[", new_domain, "] dismiss DHCP allocation: ",
				    *allocation, " (other/no DHCP server)");
			}
			_dhcp_allocations.remove(*allocation);
			_destroy_dhcp_allocation(*allocation, old_domain);
		}
	}
	if (dhcp_clients_outdated) {
		_reset_and_refetch_domain_ready_state();
	}
}


void Interface::_update_own_arp_waiters(Domain &domain)
{
	bool const verbose { _config().verbose() };
	_own_arp_waiters.for_each([&] (Arp_waiter_list_element &le)
	{
		Arp_waiter &arp_waiter { *le.object() };
		bool dismiss_arp_waiter { true };
		_config().domains().with_element(
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
	_config = config;
	_policy.handle_config(config);
	Domain_name const &new_domain_name = _policy.determine_domain_name();
	try {
		/* destroy state objects that are not needed anymore */
		Domain &old_domain = domain();
		_destroy_dissolved_links<Icmp_link>(_dissolved_icmp_links, _alloc);
		_destroy_dissolved_links<Udp_link> (_dissolved_udp_links,  _alloc);
		_destroy_dissolved_links<Tcp_link> (_dissolved_tcp_links,  _alloc);
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
	}
	catch (Pointer<Domain>::Invalid) { }
}


void Interface::_failed_to_send_packet_link()
{
	if (_config().verbose()) {
		log("[", _domain(), "] failed to send packet (link down)"); }
}


void Interface::_failed_to_send_packet_submit()
{
	if (_config().verbose()) {
		log("[", _domain(), "] failed to send packet (queue full)"); }
}


void Interface::_failed_to_send_packet_alloc()
{
	if (_config().verbose()) {
		log("[", _domain(), "] failed to send packet (packet alloc failed)"); }
}


void Interface::handle_config_2()
{
	Domain_name const &new_domain_name = _policy.determine_domain_name();
	try {
		Domain &old_domain = domain();
		_config().domains().with_element(
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
	}
	catch (Pointer<Domain>::Invalid) {

		/* the interface had no domain but now it may get one */
		_config().domains().with_element(
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
	}
}


void Interface::handle_config_3()
{
	try {
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
	}
	catch (Constructible<Update_domain>::Deref_unconstructed_object) {

		/* if the interface moved to another domain, finish the operation */
		try { attach_to_domain_finish(); }
		catch (Pointer<Domain>::Invalid) { }
	}
}


void Interface::_ack_packet(Packet_descriptor const &pkt)
{
	if (!_sink.try_ack_packet(pkt)) {
		if (_config().verbose()) {
			log("[", _domain(), "] leak packet (sink not ready to "
			    "acknowledge)");
		}
		return;
	}
}


void Interface::cancel_arp_waiting(Arp_waiter &waiter)
{
	try {
		Domain &domain = _domain();
		if (domain.verbose_packet_drop()) {
			log("[", domain, "] drop packet (ARP got cancelled)"); }
	}
	catch (Pointer<Domain>::Invalid) {
		if (_config().verbose_packet_drop()) {
			log("[?] drop packet (ARP got cancelled)"); }
	}
	_ack_packet(waiter.packet());
	destroy(_alloc, &waiter);
}


Interface::~Interface()
{
	try { _config().report().handle_interface_link_state(); }
	catch (Pointer<Report>::Invalid) { }
	_detach_from_domain();
	_interfaces.remove(this);
}


void Interface::report(Genode::Xml_generator &xml)
{
	xml.node("interface",  [&] () {
		bool empty { true };
		xml.attribute("label", _policy.label());
		if (_config().report().link_state()) {
			xml.attribute("link_state", link_state());
			empty = false;
		}
		if (_config().report().stats()) {
			try {
				_policy.report(xml);
				empty = false;
			}
			catch (Report::Empty) { }

			try { xml.node("tcp-links",        [&] () { _tcp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("udp-links",        [&] () { _udp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("icmp-links",       [&] () { _icmp_stats.report(xml); }); empty = false; } catch (Report::Empty) { }
			try { xml.node("arp-waiters",      [&] () { _arp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("dhcp-allocations", [&] () { _dhcp_stats.report(xml); }); empty = false; } catch (Report::Empty) { }
		}
		if (_config().report().dropped_fragm_ipv4() && _dropped_fragm_ipv4) {
			xml.node("dropped-fragm-ipv4", [&] () {
				xml.attribute("value", _dropped_fragm_ipv4);
			});
			empty = false;
		}
		if (empty) { throw Report::Empty(); }
	});
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
