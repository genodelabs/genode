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
#include <base/quota_guard.h>

/* local includes */
#include <interface.h>
#include <configuration.h>
#include <l3_protocol.h>

using namespace Net;
using Genode::Deallocator;
using Genode::size_t;
using Genode::uint32_t;
using Genode::log;
using Genode::Exception;
using Genode::construct_at;
using Genode::Quota_guard;
using Genode::Ram_quota;
using Genode::Constructible;
using Genode::Signal_context_capability;
using Genode::Signal_transmitter;


/***************
 ** Utilities **
 ***************/

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
	link.dissolve();
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
		static_cast<Udp_link *>(&link)->packet();
		return;
	case L3_protocol::ICMP:
		static_cast<Icmp_link *>(&link)->packet();
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


void Interface::_pass_prot(Ethernet_frame       &eth,
                           Size_guard           &size_guard,
                           Ipv4_packet          &ip,
                           L3_protocol    const  prot,
                           void          *const  prot_base,
                           size_t         const  prot_size)
{
	eth.src(_router_mac);
	_update_checksum(prot, prot_base, prot_size, ip.src(), ip.dst(), ip.total_length());
	_pass_ip(eth, size_guard, ip);
}


void Interface::_pass_ip(Ethernet_frame &eth,
                         Size_guard     &size_guard,
                         Ipv4_packet    &ip)
{
	ip.update_checksum();
	send(eth, size_guard);
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
	Signal_transmitter(_session_link_state_sigh).submit();
	_interfaces.remove(this);
	domain.attach_interface(*this);
}


void Interface::_detach_from_domain_raw()
{
	Domain &domain = _domain();
	domain.detach_interface(*this);
	_interfaces.insert(this);
	_domain = Pointer<Domain>();
	Signal_transmitter(_session_link_state_sigh).submit();
}


void Interface::attach_to_domain()
{
	try {
		_attach_to_domain_raw(_config().domains().find_by_name(
			_policy.determine_domain_name()));

		attach_to_domain_finish();
	}
	catch (Domain_tree::No_match) { }
}


void Interface::attach_to_domain_finish()
{
	if (!link_state()) {
		return; }

	/* if domain has yet no IP config, participate in requesting one */
	Domain &domain = _domain();
	Ipv4_config const &ip_config = domain.ip_config();
	if (!ip_config.valid) {
		_dhcp_client.discover();
		return;
	}
	attach_to_ip_config(domain, ip_config);
}


void Interface::attach_to_ip_config(Domain            &domain,
                                    Ipv4_config const &ip_config)
{
	/* if others wait for ARP at the domain, participate in requesting it */
	domain.foreign_arp_waiters().for_each([&] (Arp_waiter_list_element &le) {
		_broadcast_arp_request(ip_config.interface.address,
		                       le.object()->ip());
	});
}


void Interface::session_link_state_sigh(Signal_context_capability sigh)
{
	_session_link_state_sigh = sigh;
}


void Interface::detach_from_ip_config()
{
	/* destroy our own ARP waiters */
	Domain &domain = _domain();
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


void Interface::detach_from_remote_ip_config()
{
	/* only the DNS server address of the local DHCP server can be remote */
	Signal_transmitter(_session_link_state_sigh).submit();
}


void Interface::attach_to_remote_ip_config()
{
	/* only the DNS server address of the local DHCP server can be remote */
	Signal_transmitter(_session_link_state_sigh).submit();
}


void Interface::_detach_from_domain()
{
	try {
		detach_from_ip_config();
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
				           remote, _timer, _config(), protocol };
		}
		catch (Quota_guard<Ram_quota>::Limit_exceeded) {
			throw Drop_packet(
				"RAM quota exhausted during allocation of TCP link");
		}
		break;
	case L3_protocol::UDP:
		try {
			new (_alloc)
				Udp_link { *this, local, remote_port_alloc, remote_domain,
				           remote, _timer, _config(), protocol };
		}
		catch (Quota_guard<Ram_quota>::Limit_exceeded) {
			throw Drop_packet(
				"RAM quota exhausted during allocation of UDP link");
		}
		break;
	case L3_protocol::ICMP:
		try {
			new (_alloc)
				Icmp_link { *this, local, remote_port_alloc, remote_domain,
				            remote, _timer, _config(), protocol };
		}
		catch (Quota_guard<Ram_quota>::Limit_exceeded) {
			throw Drop_packet(
				"RAM quota exhausted during allocation of ICMP link");
		}
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
	if (!remote_ip_cfg.valid) {
		throw Drop_packet("target domain has yet no IP config");
	}
	Ipv4_address const &hop_ip = remote_domain.next_hop(dst_ip);
	try { eth.dst(remote_domain.arp_cache().find_by_ip(hop_ip).mac()); }
	catch (Arp_cache::No_match) {
		remote_domain.interfaces().for_each([&] (Interface &interface) {
			interface._broadcast_arp_request(remote_ip_cfg.interface.address,
			                                 hop_ip);
		});
		try { new (_alloc) Arp_waiter { *this, remote_domain, hop_ip, pkt }; }
		catch (Quota_guard<Ram_quota>::Limit_exceeded) {
			throw Drop_packet(
				"RAM quota exhausted during allocation of ARP waiter");
		}
		throw Packet_postponed();
	}
}


void Interface::_nat_link_and_pass(Ethernet_frame        &eth,
                                   Size_guard            &size_guard,
                                   Ipv4_packet           &ip,
                                   L3_protocol     const  prot,
                                   void           *const  prot_base,
                                   size_t          const  prot_size,
                                   Link_side_id    const &local_id,
                                   Domain                &local_domain,
                                   Domain                &remote_domain)
{
	Pointer<Port_allocator_guard> remote_port_alloc;
	try {
		Nat_rule &nat = remote_domain.nat_rules().find_by_domain(local_domain);
		if(_config().verbose()) {
			log("[", local_domain, "] using NAT rule: ", nat); }

		_src_port(prot, prot_base, nat.port_alloc(prot).alloc());
		ip.src(remote_domain.ip_config().interface.address);
		remote_port_alloc = nat.port_alloc(prot);
	}
	catch (Nat_rule_tree::No_match) { }
	Link_side_id const remote_id = { ip.dst(), _dst_port(prot, prot_base),
	                                 ip.src(), _src_port(prot, prot_base) };
	_new_link(prot, local_id, remote_port_alloc, remote_domain, remote_id);
	remote_domain.interfaces().for_each([&] (Interface &interface) {
		interface._pass_prot(eth, size_guard, ip, prot, prot_base, prot_size);
	});
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
		if (dhcp_srv.dns_server().valid()) {
			dhcp_opts.append_option<Dhcp_packet::Dns_server_ipv4>(dhcp_srv.dns_server()); }
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
		                 local_domain.ip_config().interface);
	}
	catch (Quota_guard<Ram_quota>::Limit_exceeded) {
		throw Drop_packet(
			"RAM quota exhausted during allocation of DHCP allocation");
	}
}


void Interface::_handle_dhcp_request(Ethernet_frame &eth,
                                     Dhcp_packet    &dhcp,
                                     Domain         &local_domain)
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

			Ipv4_address_prefix const &local_intf =
				local_domain.ip_config().interface;

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

			case Dhcp_packet::Message_type::REQUEST: throw Drop_packet("DHCP REQUEST from client without offered/acked IP");
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
	eth.src(_router_mac);
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
	return _domain.valid() && _session_link_state;
}


void Interface::handle_link_state()
{
	struct Keep_ip_config : Exception { };
	try {
		attach_to_domain_finish();

		/* if the wholde domain became down, discard IP config */
		Domain &domain_ = domain();
		if (!link_state() && domain_.ip_config().valid) {
			domain_.interfaces().for_each([&] (Interface &interface) {
				if (interface.link_state()) {
					throw Keep_ip_config(); }
			});
			domain_.discard_ip_config();
		}
	}
	catch (Domain::Ip_config_static) { }
	catch (Keep_ip_config) { }
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

	/* update checksums and send */
	icmp.update_checksum(icmp_sz - sizeof(Icmp_packet));
	ip.update_checksum();
	send(eth, size_guard);
}


void Interface::_handle_icmp_query(Ethernet_frame          &eth,
                                   Size_guard              &size_guard,
                                   Ipv4_packet             &ip,
                                   Packet_descriptor const &pkt,
                                   L3_protocol              prot,
                                   void                    *prot_base,
                                   size_t                   prot_size,
                                   Domain                  &local_domain)
{
	Link_side_id const local_id = { ip.src(), _src_port(prot, prot_base),
	                                ip.dst(), _dst_port(prot, prot_base) };

	/* try to route via existing ICMP links */
	try {
		Link_side const &local_side = local_domain.links(prot).find_by_id(local_id);
		Link &link = local_side.link();
		bool const client = local_side.is_client();
		Link_side &remote_side = client ? link.server() : link.client();
		Domain &remote_domain = remote_side.domain();
		if (_config().verbose()) {
			log("[", local_domain, "] using ", l3_protocol_name(prot),
			    " link: ", link);
		}
		_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
		ip.src(remote_side.dst_ip());
		ip.dst(remote_side.src_ip());
		_src_port(prot, prot_base, remote_side.dst_port());
		_dst_port(prot, prot_base, remote_side.src_port());

		remote_domain.interfaces().for_each([&] (Interface &interface) {
			interface._pass_prot(eth, size_guard, ip, prot, prot_base, prot_size);
		});
		_link_packet(prot, prot_base, link, client);
		return;
	}
	catch (Link_side_tree::No_match) { }

	/* try to route via ICMP rules */
	try {
		Ip_rule const &rule =
			local_domain.icmp_rules().longest_prefix_match(ip.dst());

		if(_config().verbose()) {
			log("[", local_domain, "] using ICMP rule: ", rule); }

		Domain &remote_domain = rule.domain();
		_adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
		_nat_link_and_pass(eth, size_guard, ip, prot, prot_base, prot_size,
		                   local_id, local_domain, remote_domain);

		return;
	}
	catch (Ip_rule_list::No_match) { }

	throw Bad_transport_protocol();
}


void Interface::_handle_icmp_error(Ethernet_frame          &eth,
                                   Size_guard              &size_guard,
                                   Ipv4_packet             &ip,
                                   Packet_descriptor const &pkt,
                                   Domain                  &local_domain,
                                   Icmp_packet             &icmp,
                                   size_t                   icmp_sz)
{
	/* drop packet if embedded IP checksum invalid */
	Ipv4_packet &embed_ip = icmp.data<Ipv4_packet>(size_guard);
	if (embed_ip.checksum_error()) {
		throw Drop_packet("bad checksum in IP packet embedded in ICMP error");
	}
	/* get link identity of the embeddeded transport packet */
	L3_protocol  const embed_prot      = embed_ip.protocol();
	void        *const embed_prot_base = _prot_base(embed_prot, size_guard, embed_ip);
	Link_side_id const local_id = { embed_ip.dst(), _dst_port(embed_prot, embed_prot_base),
	                                embed_ip.src(), _src_port(embed_prot, embed_prot_base) };
	try {
		/* lookup a link state that matches the embedded transport packet */
		Link_side const &local_side = local_domain.links(embed_prot).find_by_id(local_id);
		Link &link = local_side.link();
		bool const client = local_side.is_client();
		Link_side &remote_side = client ? link.server() : link.client();
		Domain &remote_domain = remote_side.domain();

		/* print out that the link is used */
		if (_config().verbose()) {
			log("[", local_domain, "] using ", l3_protocol_name(embed_prot),
			    " link: ", link);
		}
		/* adapt source and destination of Ethernet frame and IP packet */
		_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
		if (remote_side.dst_ip() == remote_domain.ip_config().interface.address) {
			ip.src(remote_side.dst_ip());
		}
		ip.dst(remote_side.src_ip());

		/* adapt source and destination of embedded IP and transport packet */
		embed_ip.src(remote_side.src_ip());
		embed_ip.dst(remote_side.dst_ip());
		_src_port(embed_prot, embed_prot_base, remote_side.src_port());
		_dst_port(embed_prot, embed_prot_base, remote_side.dst_port());

		/* update checksum of both IP headers and the ICMP header */
		embed_ip.update_checksum();
		icmp.update_checksum(icmp_sz - sizeof(Icmp_packet));
		ip.update_checksum();

		/* send adapted packet to all interfaces of remote domain */
		remote_domain.interfaces().for_each([&] (Interface &interface) {
			interface.send(eth, size_guard);
		});
		/* refresh link only if the error is not about an ICMP query */
		if (embed_prot != L3_protocol::ICMP) {
			_link_packet(embed_prot, embed_prot_base, link, client); }
	}
	/* drop packet if there is no matching link */
	catch (Link_side_tree::No_match) {
		throw Drop_packet("no link that matches packet embedded in ICMP error"); }
}


void Interface::_handle_icmp(Ethernet_frame            &eth,
                             Size_guard                &size_guard,
                             Ipv4_packet               &ip,
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
	case Icmp_packet::Type::ECHO_REQUEST:    _handle_icmp_query(eth, size_guard, ip, pkt, prot, prot_base, prot_size, local_domain); break;
	case Icmp_packet::Type::DST_UNREACHABLE: _handle_icmp_error(eth, size_guard, ip, pkt, local_domain, icmp, prot_size); break;
	default: Drop_packet("unhandled type in ICMP"); }
}


void Interface::_handle_ip(Ethernet_frame          &eth,
                           Size_guard              &size_guard,
                           Packet_descriptor const &pkt,
                           Domain                  &local_domain)
{
	/* read packet information */
	Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
	Ipv4_address_prefix const &local_intf = local_domain.ip_config().interface;

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
				if (dhcp.op() == Dhcp_packet::REQUEST) {
					try {
						_handle_dhcp_request(eth, dhcp, local_domain);
						return;
					}
					catch (Pointer<Dhcp_server>::Invalid) { }
				} else {
					_dhcp_client.handle_ip(eth, size_guard);
					return;
				}
			}
		}
		else if (prot == L3_protocol::ICMP) {
			_handle_icmp(eth, size_guard, ip, pkt, prot, prot_base, prot_size,
			             local_domain, local_intf);
			return;
		}

		Link_side_id const local_id = { ip.src(), _src_port(prot, prot_base),
		                                ip.dst(), _dst_port(prot, prot_base) };

		/* try to route via existing UDP/TCP links */
		try {
			Link_side const &local_side = local_domain.links(prot).find_by_id(local_id);
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Domain &remote_domain = remote_side.domain();
			if (_config().verbose()) {
				log("[", local_domain, "] using ", l3_protocol_name(prot),
				    " link: ", link);
			}
			_adapt_eth(eth, remote_side.src_ip(), pkt, remote_domain);
			ip.src(remote_side.dst_ip());
			ip.dst(remote_side.src_ip());
			_src_port(prot, prot_base, remote_side.dst_port());
			_dst_port(prot, prot_base, remote_side.src_port());

			remote_domain.interfaces().for_each([&] (Interface &interface) {
				interface._pass_prot(eth, size_guard, ip, prot, prot_base, prot_size);
			});
			_link_packet(prot, prot_base, link, client);
			return;
		}
		catch (Link_side_tree::No_match) { }

		/* try to route via forward rules */
		if (local_id.dst_ip == local_intf.address) {
			try {
				Forward_rule const &rule =
					_forward_rules(local_domain, prot).find_by_port(local_id.dst_port);

				if(_config().verbose()) {
					log("[", local_domain, "] using forward rule: ",
					    l3_protocol_name(prot), " ", rule);
				}
				Domain &remote_domain = rule.domain();
				_adapt_eth(eth, rule.to(), pkt, remote_domain);
				ip.dst(rule.to());
				_nat_link_and_pass(eth, size_guard, ip, prot, prot_base,
				                   prot_size, local_id, local_domain, remote_domain);
				return;
			}
			catch (Forward_rule_tree::No_match) { }
		}
		/* try to route via transport and permit rules */
		try {
			Transport_rule const &transport_rule =
				_transport_rules(local_domain, prot).longest_prefix_match(local_id.dst_ip);

			Permit_rule const &permit_rule =
				transport_rule.permit_rule(local_id.dst_port);

			if(_config().verbose()) {
				log("[", local_domain, "] using ", l3_protocol_name(prot),
				    " rule: ", transport_rule, " ", permit_rule);
			}
			Domain &remote_domain = permit_rule.domain();
			_adapt_eth(eth, local_id.dst_ip, pkt, remote_domain);
			_nat_link_and_pass(eth, size_guard, ip, prot, prot_base, prot_size,
			                   local_id, local_domain, remote_domain);
			return;
		}
		catch (Transport_rule_list::No_match) { }
		catch (Permit_single_rule_tree::No_match) { }
	}
	catch (Interface::Bad_transport_protocol) { }

	/* try to route via IP rules */
	try {
		Ip_rule const &rule =
			local_domain.ip_rules().longest_prefix_match(ip.dst());

		if(_config().verbose()) {
			log("[", local_domain, "] using IP rule: ", rule); }

		Domain &remote_domain = rule.domain();
		_adapt_eth(eth, ip.dst(), pkt, remote_domain);
		remote_domain.interfaces().for_each([&] (Interface &interface) {
			interface._pass_ip(eth, size_guard, ip);
		});

		return;
	}
	catch (Ip_rule_list::No_match) { }

	/* give up and drop packet */
	_send_icmp_dst_unreachable(local_intf, eth, ip,
	                           Icmp_packet::Code::DST_NET_UNREACHABLE);
	if (_config().verbose()) {
		log("[", local_domain, "] unroutable packet"); }
}


void Interface::_broadcast_arp_request(Ipv4_address const &src_ip,
                                       Ipv4_address const &dst_ip)
{
	enum {
		ETH_HDR_SZ = sizeof(Ethernet_frame),
		ETH_DAT_SZ = sizeof(Arp_packet) + ETH_HDR_SZ >= Ethernet_frame::MIN_SIZE ?
		             sizeof(Arp_packet) :
		             Ethernet_frame::MIN_SIZE - ETH_HDR_SZ,
		ETH_CRC_SZ = sizeof(Genode::uint32_t),
		PKT_SIZE   = ETH_HDR_SZ + ETH_DAT_SZ + ETH_CRC_SZ,
	};
	send(PKT_SIZE, [&] (void *pkt_base, Size_guard &size_guard) {

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
	try {
		/* check wether a matching ARP cache entry already exists */
		local_domain.arp_cache().find_by_ip(arp.src_ip());
		if (_config().verbose()) {
			log("[", local_domain, "] ARP entry already exists"); }
	}
	catch (Arp_cache::No_match) {

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
	Ipv4_address_prefix const &local_intf = local_domain.ip_config().interface;
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


void Interface::_send_arp_reply(Ethernet_frame &eth,
                                Size_guard     &size_guard,
                                Arp_packet     &arp)
{
	/* interchange source and destination MAC and IP addresses */
	Ipv4_address dst_ip = arp.dst_ip();
	arp.dst_ip(arp.src_ip());
	arp.dst_mac(arp.src_mac());
	eth.dst(eth.src());
	arp.src_ip(dst_ip);
	arp.src_mac(_router_mac);
	eth.src(_router_mac);

	/* mark packet as reply and send it back to its sender */
	arp.opcode(Arp_packet::REPLY);
	send(eth, size_guard);
}


void Interface::_handle_arp_request(Ethernet_frame &eth,
                                    Size_guard     &size_guard,
                                    Arp_packet     &arp,
                                    Domain         &local_domain)
{
	Ipv4_config         const &local_ip_cfg = local_domain.ip_config();
	Ipv4_address_prefix const &local_intf   = local_ip_cfg.interface;
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
			_send_arp_reply(eth, size_guard, arp);

		} else {

			/* forward request to all other interfaces of the domain */
			if (_config().verbose()) {
				log("[", local_domain, "] forward ARP request for local IP "
				    "to all interfaces of the sender domain"); }
			_domain_broadcast(eth, size_guard, local_domain);
		}

	} else {

		/* ARP request for an IP foreign to the domain's subnet */
		if (local_ip_cfg.gateway_valid) {

			/* leave request up to the gateway of the domain */
			throw Drop_packet("leave ARP request up to gateway");

		} else {

			/* try to act as gateway for the domain as none is configured */
			if (_config().verbose()) {
				log("[", local_domain, "] answer ARP request for foreign IP "
				    "with router MAC"); }
			_send_arp_reply(eth, size_guard, arp);
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


void Interface::_ready_to_submit()
{
	while (_sink.packet_avail()) {
		Packet_descriptor const pkt = _sink.get_packet();
		Size_guard size_guard(pkt.size());
		try { _handle_eth(_sink.packet_content(pkt), size_guard, pkt); }
		catch (Packet_postponed) { continue; }
		_ack_packet(pkt);
	}
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
	_ack_packet(pkt);
}


void Interface::_ready_to_ack()
{
	while (_source.ack_avail()) {
		_source.release_packet(_source.get_acked_packet()); }
}


void Interface::_destroy_dhcp_allocation(Dhcp_allocation &allocation,
                                         Domain          &local_domain)
{
	try { local_domain.dhcp_server().free_ip(allocation.ip()); }
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

				if (local_domain.verbose_packets()) {
					log("[", local_domain, "] rcv ", eth); }

				if (local_domain.ip_config().valid) {

					switch (eth.type()) {
					case Ethernet_frame::Type::ARP:  _handle_arp(eth, size_guard, local_domain);     break;
					case Ethernet_frame::Type::IPV4: _handle_ip(eth, size_guard, pkt, local_domain); break;
					default: throw Bad_network_protocol(); }

				} else {

					switch (eth.type()) {
					case Ethernet_frame::Type::IPV4: _dhcp_client.handle_ip(eth, size_guard); break;
					default: throw Bad_network_protocol(); }
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


void Interface::_send_alloc_pkt(Packet_descriptor &pkt,
                                void            * &pkt_base,
                                size_t             pkt_size)
{
	pkt      = _source.alloc_packet(pkt_size);
	pkt_base = _source.packet_content(pkt);
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
	_source.submit_packet(pkt);
}


Interface::Interface(Genode::Entrypoint     &ep,
                     Timer::Connection      &timer,
                     Mac_address      const  router_mac,
                     Genode::Allocator      &alloc,
                     Mac_address      const  mac,
                     Configuration          &config,
                     Interface_list         &interfaces,
                     Packet_stream_sink     &sink,
                     Packet_stream_source   &source,
                     bool                   &session_link_state,
                     Interface_policy       &policy)
:
	_sink               { sink },
	_source             { source },
	_session_link_state { session_link_state },
	_sink_ack           { ep, *this, &Interface::_ack_avail },
	_sink_submit        { ep, *this, &Interface::_ready_to_submit },
	_source_ack         { ep, *this, &Interface::_ready_to_ack },
	_source_submit      { ep, *this, &Interface::_packet_avail },
	_router_mac         { router_mac },
	_mac                { mac },
	_config             { config },
	_policy             { policy },
	_timer              { timer },
	_alloc              { alloc },
	_interfaces         { interfaces }
{
	_interfaces.insert(this);
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
		if (link.server().dst_ip() != new_srv_dom.ip_config().interface.address) {
			_dismiss_link_log(link, "NAT IP");
			throw Dismiss_link();
		}
		Nat_rule &nat = new_srv_dom.nat_rules().find_by_domain(cln_dom);
		Port_allocator_guard &remote_port_alloc = nat.port_alloc(prot);
		remote_port_alloc.alloc(link.server().dst_port());
		remote_port_alloc_ptr = remote_port_alloc;
		link.handle_config(cln_dom, new_srv_dom, remote_port_alloc_ptr, _config());
		return;
	}
	catch (Nat_rule_tree::No_match)              { _dismiss_link_log(link, "no NAT rule"); }
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
			Forward_rule const &rule =
				_forward_rules(cln_dom, prot).
					find_by_port(link.client().dst_port());

			/* if destination IP of forwarding changed, dismiss link */
			if (rule.to() != link.server().src_ip()) {
				_dismiss_link_log(link, "other forward-rule to");
				throw Dismiss_link();
			}
			_update_link_check_nat(link, rule.domain(), prot, cln_dom);
			return;
		}
		catch (Forward_rule_tree::No_match) {
			try {
				/* try to find transport rule that matches the server IP */
				Transport_rule const &transport_rule =
					_transport_rules(cln_dom, prot).
						longest_prefix_match(link.client().dst_ip());

				/* try to find permit rule that matches the server port */
				Permit_rule const &permit_rule =
					transport_rule.permit_rule(link.client().dst_port());

				_update_link_check_nat(link, permit_rule.domain(), prot, cln_dom);
				return;
			}
			catch (Transport_rule_list::No_match)     { _dismiss_link_log(link, "no transport/forward rule"); }
			catch (Permit_single_rule_tree::No_match) { _dismiss_link_log(link, "no permit rule"); }
			catch (Dismiss_link)                      { }
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
			Ip_rule const &rule = cln_dom.icmp_rules().
				longest_prefix_match(link.client().dst_ip());

			_update_link_check_nat(link, rule.domain(), prot, cln_dom);
			return;
		}
		catch (Ip_rule_list::No_match) { _dismiss_link_log(link, "no ICMP rule"); }
		catch (Dismiss_link) { }
		_destroy_link(link);
	});
}


void Interface::_update_dhcp_allocations(Domain &old_domain,
                                         Domain &new_domain)
{
	try {
		Dhcp_server &old_dhcp_srv = old_domain.dhcp_server();
		Dhcp_server &new_dhcp_srv = new_domain.dhcp_server();
		if (old_dhcp_srv.dns_server() != new_dhcp_srv.dns_server()) {
			throw Pointer<Dhcp_server>::Invalid();
		}
		if (old_dhcp_srv.ip_lease_time().value !=
		    new_dhcp_srv.ip_lease_time().value)
		{
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
			_dhcp_allocations.remove(allocation);
			_destroy_dhcp_allocation(allocation, old_domain);
		});
	}
	catch (Pointer<Dhcp_server>::Invalid) {

		/* dismiss all DHCP allocations */
		while (Dhcp_allocation *allocation = _dhcp_allocations.first()) {
			if (_config().verbose()) {
				log("[", new_domain, "] dismiss DHCP allocation: ",
				    *allocation, " (other/no DHCP server)");
			}
			_dhcp_allocations.remove(*allocation);
			_destroy_dhcp_allocation(*allocation, old_domain);
		}
	}
}


void Interface::_update_own_arp_waiters(Domain &domain)
{
	bool const verbose = _config().verbose();
	_own_arp_waiters.for_each([&] (Arp_waiter_list_element &le) {
		Arp_waiter &arp_waiter = *le.object();
		try {
			Domain &dst = _config().domains().find_by_name(arp_waiter.dst().name());
			if (dst.ip_config() != arp_waiter.dst().ip_config()) {
				if (verbose) {
					log("[", domain, "] dismiss ARP waiter: ", arp_waiter,
					    " (IP config changed)");
				}
				throw Dismiss_arp_waiter();
			}
			/* keep ARP waiter */
			arp_waiter.handle_config(dst);
			if (verbose) {
				log("[", domain, "] update ARP waiter: ", arp_waiter);
			}
			return;
		}
		/* dismiss ARP waiter */
		catch (Domain_tree::No_match) {
			if (verbose) {
				log("[", domain, "] dismiss ARP waiter: ", arp_waiter,
				    " (domain disappeared)");
			}
		}
		catch (Dismiss_arp_waiter) { }
		cancel_arp_waiting(*_own_arp_waiters.first()->object());
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
		Domain &new_domain = config.domains().find_by_name(new_domain_name);
		new_domain.try_reuse_ip_config(old_domain);
		return;
	}
	catch (Domain_tree::No_match) { }
	catch (Pointer<Domain>::Invalid) { }
}


void Interface::_failed_to_send_packet_link()
{
	if (_config().verbose()) {
		log("[", _domain(), "] failed to send packet (link down)"); }
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
		/* if the domains differ, detach completely from the domain */
		Domain &old_domain = domain();
		Domain &new_domain = _config().domains().find_by_name(new_domain_name);
		if (old_domain.name() != new_domain_name) {
			_detach_from_domain();
			_attach_to_domain_raw(new_domain);
			return;
		}
		/* move to new domain object without considering any state objects */
		_detach_from_domain_raw();
		_attach_to_domain_raw(new_domain);

		/* remember that the interface stays attached to the same domain */
		_update_domain.construct(old_domain, new_domain);
		return;
	}
	catch (Domain_tree::No_match) {

		/* the interface no longer has a domain */
		_detach_from_domain();
	}
	catch (Pointer<Domain>::Invalid) {

		/* the interface had no domain but now it may get one */
		try { _attach_to_domain_raw(_config().domains().find_by_name(new_domain_name)); }
		catch (Domain_tree::No_match) { }
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
			detach_from_ip_config();
			return;
		}
		/* if there was/is no IP config, there is nothing more to update */
		if (!new_domain.ip_config().valid) {
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
	if (!_sink.ready_to_ack()) {
		if (_config().verbose()) {
			log("[", _domain(), "] leak packet (sink not ready to "
			    "acknowledge)");
		}
		return;
	}
	_sink.acknowledge_packet(pkt);
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
	_detach_from_domain();
	_interfaces.remove(this);
}
