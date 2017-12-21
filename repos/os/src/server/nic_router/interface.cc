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
#include <net/arp.h>

/* local includes */
#include <interface.h>
#include <configuration.h>
#include <l3_protocol.h>
#include <size_guard.h>

using namespace Net;
using Genode::Deallocator;
using Genode::size_t;
using Genode::uint32_t;
using Genode::log;
using Genode::error;
using Genode::warning;


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
static void _destroy_links(Link_list   &links,
                           Link_list   &dissolved_links,
                           Deallocator &dealloc)
{
	_destroy_dissolved_links<LINK_TYPE>(dissolved_links, dealloc);
	while (Link *link = links.first()) {
		link->dissolve();
		links.remove(link);
		destroy(dealloc, static_cast<LINK_TYPE *>(link));
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
		static_cast<Udp_link *>(&link)->packet();
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void _update_checksum(L3_protocol   const prot,
                             void         *const prot_base,
                             size_t        const prot_size,
                             Ipv4_address  const src,
                             Ipv4_address  const dst)
{
	switch (prot) {
	case L3_protocol::TCP:
		((Tcp_packet *)prot_base)->update_checksum(src, dst, prot_size);
		return;
	case L3_protocol::UDP:
		((Udp_packet *)prot_base)->update_checksum(src, dst);
		return;
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _dst_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP: return (*(Tcp_packet *)prot_base).dst_port();
	case L3_protocol::UDP: return (*(Udp_packet *)prot_base).dst_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _dst_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP: (*(Tcp_packet *)prot_base).dst_port(port); return;
	case L3_protocol::UDP: (*(Udp_packet *)prot_base).dst_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static Port _src_port(L3_protocol const prot, void *const prot_base)
{
	switch (prot) {
	case L3_protocol::TCP: return (*(Tcp_packet *)prot_base).src_port();
	case L3_protocol::UDP: return (*(Udp_packet *)prot_base).src_port();
	default: throw Interface::Bad_transport_protocol(); }
}


static void _src_port(L3_protocol  const prot,
                      void        *const prot_base,
                      Port         const port)
{
	switch (prot) {
	case L3_protocol::TCP: ((Tcp_packet *)prot_base)->src_port(port); return;
	case L3_protocol::UDP: ((Udp_packet *)prot_base)->src_port(port); return;
	default: throw Interface::Bad_transport_protocol(); }
}


static void *_prot_base(L3_protocol const  prot,
                        size_t      const  prot_size,
                        Ipv4_packet       &ip)
{
	switch (prot) {
	case L3_protocol::TCP:
		Tcp_packet::validate_size(prot_size);
		return ip.data<Tcp_packet>();
	case L3_protocol::UDP:
		Udp_packet::validate_size(prot_size);
		return ip.data<Udp_packet>();
	default: throw Interface::Bad_transport_protocol(); }
}


/***************
 ** Interface **
 ***************/

void Interface::_pass_prot(Ethernet_frame       &eth,
                           size_t         const  eth_size,
                           Ipv4_packet          &ip,
                           L3_protocol    const  prot,
                           void          *const  prot_base,
                           size_t         const  prot_size)
{
	_update_checksum(prot, prot_base, prot_size, ip.src(), ip.dst());
	_pass_ip(eth, eth_size, ip);
}


void Interface::_pass_ip(Ethernet_frame &eth,
                         size_t   const  eth_size,
                         Ipv4_packet    &ip)
{
	ip.checksum(Ipv4_packet::calculate_checksum(ip));
	send(eth, eth_size);
}


Forward_rule_tree &
Interface::_forward_rules(L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return _domain.tcp_forward_rules();
	case L3_protocol::UDP: return _domain.udp_forward_rules();
	default: throw Bad_transport_protocol(); }
}


Transport_rule_list &
Interface::_transport_rules(L3_protocol const prot) const
{
	switch (prot) {
	case L3_protocol::TCP: return _domain.tcp_rules();
	case L3_protocol::UDP: return _domain.udp_rules();
	default: throw Bad_transport_protocol(); }
}


void
Interface::_new_link(L3_protocol                   const  protocol,
                     Link_side_id                  const &local,
                     Pointer<Port_allocator_guard> const  remote_port_alloc,
                     Domain                              &remote_domain,
                     Link_side_id                  const &remote)
{
	switch (protocol) {
	case L3_protocol::TCP:
		new (_alloc) Tcp_link(*this, local, remote_port_alloc, remote_domain,
		                      remote, _timer, _config(), protocol);
		break;
	case L3_protocol::UDP:
		new (_alloc) Udp_link(*this, local, remote_port_alloc, remote_domain,
		                      remote, _timer, _config(), protocol);
		break;
	default: throw Bad_transport_protocol(); }
}


void Interface::dhcp_allocation_expired(Dhcp_allocation &allocation)
{
	_release_dhcp_allocation(allocation);
	_released_dhcp_allocations.insert(&allocation);
}


Link_list &Interface::links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP: return _tcp_links;
	case L3_protocol::UDP: return _udp_links;
	default: throw Bad_transport_protocol(); }
}


Link_list &Interface::dissolved_links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP: return _dissolved_tcp_links;
	case L3_protocol::UDP: return _dissolved_udp_links;
	default: throw Bad_transport_protocol(); }
}


void Interface::_adapt_eth(Ethernet_frame          &eth,
                           size_t                  ,
                           Ipv4_address      const &ip,
                           Packet_descriptor const &pkt,
                           Domain                  &domain)
{
	if (!domain.ip_config().valid) {
		throw Drop_packet_inform("target domain has yet no IP config");
	}
	Ipv4_address const &hop_ip = domain.next_hop(ip);
	try { eth.dst(domain.arp_cache().find_by_ip(hop_ip).mac()); }
	catch (Arp_cache::No_match) {
		domain.interfaces().for_each([&] (Interface &interface) {
			interface._broadcast_arp_request(hop_ip);
		});
		new (_alloc) Arp_waiter(*this, domain, hop_ip, pkt);
		throw Packet_postponed();
	}
	eth.src(_router_mac);
}


void Interface::_nat_link_and_pass(Ethernet_frame        &eth,
                                   size_t          const  eth_size,
                                   Ipv4_packet           &ip,
                                   L3_protocol     const  prot,
                                   void           *const  prot_base,
                                   size_t          const  prot_size,
                                   Link_side_id    const &local,
                                   Domain                &domain)
{
	Pointer<Port_allocator_guard> remote_port_alloc;
	try {
		Nat_rule &nat = domain.nat_rules().find_by_domain(_domain);
		if(_config().verbose()) {
			log("Using NAT rule: ", nat); }

		_src_port(prot, prot_base, nat.port_alloc(prot).alloc());
		ip.src(domain.ip_config().interface.address);
		remote_port_alloc.set(nat.port_alloc(prot));
	}
	catch (Nat_rule_tree::No_match) { }
	Link_side_id const remote = { ip.dst(), _dst_port(prot, prot_base),
	                              ip.src(), _src_port(prot, prot_base) };
	_new_link(prot, local, remote_port_alloc, domain, remote);
	domain.interfaces().for_each([&] (Interface &interface) {
		interface._pass_prot(eth, eth_size, ip, prot, prot_base, prot_size);
	});
}


void Interface::_send_dhcp_reply(Dhcp_server               const &dhcp_srv,
                                 Mac_address               const &client_mac,
                                 Ipv4_address              const &client_ip,
                                 Dhcp_packet::Message_type        msg_type,
                                 uint32_t                         xid)
{
	enum { PKT_SIZE = 512 };
	using Size_guard = Genode::Size_guard_tpl<PKT_SIZE, Dhcp_msg_buffer_too_small>;
	send(PKT_SIZE, [&] (void *pkt_base) {

		/* create ETH header of the reply */
		Size_guard size;
		size.add(sizeof(Ethernet_frame));
		Ethernet_frame &eth = *reinterpret_cast<Ethernet_frame *>(pkt_base);
		eth.dst(client_mac);
		eth.src(_router_mac);
		eth.type(Ethernet_frame::Type::IPV4);

		/* create IP header of the reply */
		enum { IPV4_TIME_TO_LIVE = 64 };
		size_t const ip_off = size.curr();
		size.add(sizeof(Ipv4_packet));
		Ipv4_packet &ip = *eth.data<Ipv4_packet>();
		ip.header_length(sizeof(Ipv4_packet) / 4);
		ip.version(4);
		ip.diff_service(0);
		ip.identification(0);
		ip.flags(0);
		ip.fragment_offset(0);
		ip.time_to_live(IPV4_TIME_TO_LIVE);
		ip.protocol(Ipv4_packet::Protocol::UDP);
		ip.src(_router_ip());
		ip.dst(client_ip);

		/* create UDP header of the reply */
		size_t const udp_off = size.curr();
		size.add(sizeof(Udp_packet));
		Udp_packet &udp = *ip.data<Udp_packet>();
		udp.src_port(Port(Dhcp_packet::BOOTPS));
		udp.dst_port(Port(Dhcp_packet::BOOTPC));

		/* create mandatory DHCP fields of the reply  */
		size.add(sizeof(Dhcp_packet));
		Dhcp_packet &dhcp = *udp.data<Dhcp_packet>();
		dhcp.op(Dhcp_packet::REPLY);
		dhcp.htype(Dhcp_packet::Htype::ETH);
		dhcp.hlen(sizeof(Mac_address));
		dhcp.hops(0);
		dhcp.xid(xid);
		dhcp.secs(0);
		dhcp.flags(0);
		dhcp.ciaddr(msg_type == Dhcp_packet::Message_type::INFORM ? client_ip : Ipv4_address());
		dhcp.yiaddr(msg_type == Dhcp_packet::Message_type::INFORM ? Ipv4_address() : client_ip);
		dhcp.siaddr(_router_ip());
		dhcp.giaddr(Ipv4_address());
		dhcp.client_mac(client_mac);
		dhcp.zero_fill_sname();
		dhcp.zero_fill_file();
		dhcp.default_magic_cookie();

		/* append DHCP option fields to the reply */
		Dhcp_packet::Options_aggregator<Size_guard> dhcp_opts(dhcp, size);
		dhcp_opts.append_option<Dhcp_packet::Message_type_option>(msg_type);
		dhcp_opts.append_option<Dhcp_packet::Server_ipv4>(_router_ip());
		dhcp_opts.append_option<Dhcp_packet::Ip_lease_time>(dhcp_srv.ip_lease_time().value / 1000 / 1000);
		dhcp_opts.append_option<Dhcp_packet::Subnet_mask>(_ip_config().interface.subnet_mask());
		dhcp_opts.append_option<Dhcp_packet::Router_ipv4>(_router_ip());
		if (dhcp_srv.dns_server().valid()) {
			dhcp_opts.append_option<Dhcp_packet::Dns_server_ipv4>(dhcp_srv.dns_server()); }
		dhcp_opts.append_option<Dhcp_packet::Broadcast_addr>(_ip_config().interface.broadcast_address());
		dhcp_opts.append_option<Dhcp_packet::Options_end>();

		/* fill in header values that need the packet to be complete already */
		udp.length(size.curr() - udp_off);
		udp.update_checksum(ip.src(), ip.dst());
		ip.total_length(size.curr() - ip_off);
		ip.checksum(Ipv4_packet::calculate_checksum(ip));
	});
}


void Interface::_release_dhcp_allocation(Dhcp_allocation &allocation)
{
	if (_config().verbose()) {
		log("Release DHCP allocation: ", allocation, " at ", _domain);
	}
	_dhcp_allocations.remove(&allocation);
}


void Interface::_new_dhcp_allocation(Ethernet_frame &eth,
                                     Dhcp_packet    &dhcp,
                                     Dhcp_server    &dhcp_srv)
{
	Dhcp_allocation &allocation = *new (_alloc)
		Dhcp_allocation(*this, dhcp_srv.alloc_ip(),
		                dhcp.client_mac(), _timer,
		                _config().dhcp_offer_timeout());

	_dhcp_allocations.insert(&allocation);
	if (_config().verbose()) {
		log("Offer DHCP allocation: ", allocation,
		                       " at ", _domain);
	}
	_send_dhcp_reply(dhcp_srv, eth.src(),
	                 allocation.ip(),
	                 Dhcp_packet::Message_type::OFFER,
	                 dhcp.xid());
	return;
}


void Interface::_handle_dhcp_request(Ethernet_frame &eth, size_t ,
                                     Dhcp_packet    &dhcp)
{
	try {
		/* try to get the DHCP server config of this interface */
		Dhcp_server &dhcp_srv = _domain.dhcp_server();

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

					_release_dhcp_allocation(allocation);
					_destroy_dhcp_allocation(allocation);
					_new_dhcp_allocation(eth, dhcp, dhcp_srv);
					return;

				} else {
					allocation.lifetime(_config().dhcp_offer_timeout());
					_send_dhcp_reply(dhcp_srv, eth.src(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::OFFER,
					                 dhcp.xid());
					return;
				}
			case Dhcp_packet::Message_type::REQUEST:

				if (allocation.bound()) {
					allocation.lifetime(dhcp_srv.ip_lease_time());
					_send_dhcp_reply(dhcp_srv, eth.src(),
					                 allocation.ip(),
					                 Dhcp_packet::Message_type::ACK,
					                 dhcp.xid());
					return;

				} else {
					Dhcp_packet::Server_ipv4 &dhcp_srv_ip =
						dhcp.option<Dhcp_packet::Server_ipv4>();

					if (dhcp_srv_ip.value() == _router_ip()) {

						allocation.set_bound();
						allocation.lifetime(dhcp_srv.ip_lease_time());
						if (_config().verbose()) {
							log("Bind DHCP allocation: ", allocation,
							                      " at ", _domain);
						}
						_send_dhcp_reply(dhcp_srv, eth.src(),
						                 allocation.ip(),
						                 Dhcp_packet::Message_type::ACK,
						                 dhcp.xid());
						return;

					} else {

						_release_dhcp_allocation(allocation);
						_destroy_dhcp_allocation(allocation);
						return;
					}
				}
			case Dhcp_packet::Message_type::INFORM:

				_send_dhcp_reply(dhcp_srv, eth.src(),
				                 allocation.ip(),
				                 Dhcp_packet::Message_type::ACK,
				                 dhcp.xid());
				return;

			case Dhcp_packet::Message_type::DECLINE:
			case Dhcp_packet::Message_type::RELEASE:

				_release_dhcp_allocation(allocation);
				_destroy_dhcp_allocation(allocation);
				return;

			case Dhcp_packet::Message_type::NAK:   throw Drop_packet_warn("DHCP NAK from client");
			case Dhcp_packet::Message_type::OFFER: throw Drop_packet_warn("DHCP OFFER from client");
			case Dhcp_packet::Message_type::ACK:   throw Drop_packet_warn("DHCP ACK from client");
			default:                               throw Drop_packet_warn("DHCP request with broken message type");
			}
		}
		catch (Dhcp_allocation_tree::No_match) {

			switch (msg_type) {
			case Dhcp_packet::Message_type::DISCOVER:

				_new_dhcp_allocation(eth, dhcp, dhcp_srv);
				return;

			case Dhcp_packet::Message_type::REQUEST: throw Drop_packet_warn("DHCP REQUEST from client without offered/acked IP");
			case Dhcp_packet::Message_type::DECLINE: throw Drop_packet_warn("DHCP DECLINE from client without offered/acked IP");
			case Dhcp_packet::Message_type::RELEASE: throw Drop_packet_warn("DHCP RELEASE from client without offered/acked IP");
			case Dhcp_packet::Message_type::NAK:     throw Drop_packet_warn("DHCP NAK from client");
			case Dhcp_packet::Message_type::OFFER:   throw Drop_packet_warn("DHCP OFFER from client");
			case Dhcp_packet::Message_type::ACK:     throw Drop_packet_warn("DHCP ACK from client");
			default:                                 throw Drop_packet_warn("DHCP request with broken message type");
			}
		}
	}
	catch (Dhcp_packet::Option_not_found exception) {

		throw Drop_packet_warn("DHCP request misses required option ",
		                          (unsigned long)exception.code);
	}
}


void Interface::_domain_broadcast(Ethernet_frame &eth, size_t eth_size)
{
	eth.src(_router_mac);
	_domain.interfaces().for_each([&] (Interface &interface) {
		if (&interface != this) {
			interface.send(eth, eth_size);
		}
	});
}


void Interface::_handle_ip(Ethernet_frame          &eth,
                           Genode::size_t    const  eth_size,
                           Packet_descriptor const &pkt)
{
	/* read packet information */
	Ipv4_packet &ip = *eth.data<Ipv4_packet>();
	Ipv4_packet::validate_size(eth_size - sizeof(Ethernet_frame));

	/* try handling subnet-local IP packets */
	if (_ip_config().interface.prefix_matches(ip.dst()) &&
	    ip.dst() != _router_ip())
	{
		/*
		 * Packet targets IP local to the domain's subnet and doesn't target
		 * the router. Thus, forward it to all other interfaces of the domain.
		 */
		_domain_broadcast(eth, eth_size);
		return;
	}

	/* try to route via transport layer rules */
	try {
		L3_protocol  const prot      = ip.protocol();
		size_t       const prot_size = ip.total_length() - ip.header_length() * 4;
		void        *const prot_base = _prot_base(prot, prot_size, ip);

		/* try handling DHCP requests before trying any routing */
		if (prot == L3_protocol::UDP) {
			Udp_packet &udp = *ip.data<Udp_packet>();
			Udp_packet::validate_size(eth_size - sizeof(Ethernet_frame)
			                                   - sizeof(Ipv4_packet));

			if (Dhcp_packet::is_dhcp(&udp)) {

				/* get DHCP packet */
				Dhcp_packet &dhcp = *udp.data<Dhcp_packet>();
				Dhcp_packet::validate_size(eth_size - sizeof(Ethernet_frame)
				                                    - sizeof(Ipv4_packet)
				                                    - sizeof(Udp_packet));

				if (dhcp.op() == Dhcp_packet::REQUEST) {
					try {
						_handle_dhcp_request(eth, eth_size, dhcp);
						return;
					}
					catch (Pointer<Dhcp_server>::Invalid) { }
				} else {
					_dhcp_client.handle_ip(eth, eth_size);
					return;
				}
			}
		}
		Link_side_id const local = { ip.src(), _src_port(prot, prot_base),
		                             ip.dst(), _dst_port(prot, prot_base) };

		/* try to route via existing UDP/TCP links */
		try {
			Link_side const &local_side = _domain.links(prot).find_by_id(local);
			Link &link = local_side.link();
			bool const client = local_side.is_client();
			Link_side &remote_side = client ? link.server() : link.client();
			Domain &domain = remote_side.domain();
			if (_config().verbose()) {
				log("Using ", l3_protocol_name(prot), " link: ", link); }

			_adapt_eth(eth, eth_size, remote_side.src_ip(), pkt, domain);
			ip.src(remote_side.dst_ip());
			ip.dst(remote_side.src_ip());
			_src_port(prot, prot_base, remote_side.dst_port());
			_dst_port(prot, prot_base, remote_side.src_port());

			domain.interfaces().for_each([&] (Interface &interface) {
				interface._pass_prot(eth, eth_size, ip, prot, prot_base, prot_size);
			});
			_link_packet(prot, prot_base, link, client);
			return;
		}
		catch (Link_side_tree::No_match) { }

		/* try to route via forward rules */
		if (local.dst_ip == _router_ip()) {
			try {
				Forward_rule const &rule =
					_forward_rules(prot).find_by_port(local.dst_port);

				if(_config().verbose()) {
					log("Using forward rule: ", l3_protocol_name(prot), " ", rule); }

				Domain &domain = rule.domain();
				_adapt_eth(eth, eth_size, rule.to(), pkt, domain);
				ip.dst(rule.to());
				_nat_link_and_pass(eth, eth_size, ip, prot, prot_base,
				                   prot_size, local, domain);
				return;
			}
			catch (Forward_rule_tree::No_match) { }
		}
		/* try to route via transport and permit rules */
		try {
			Transport_rule const &transport_rule =
				_transport_rules(prot).longest_prefix_match(local.dst_ip);

			Permit_rule const &permit_rule =
				transport_rule.permit_rule(local.dst_port);

			if(_config().verbose()) {
				log("Using ", l3_protocol_name(prot), " rule: ", transport_rule,
				    " ", permit_rule); }

			Domain &domain = permit_rule.domain();
			_adapt_eth(eth, eth_size, local.dst_ip, pkt, domain);
			_nat_link_and_pass(eth, eth_size, ip, prot, prot_base, prot_size,
			                   local, domain);
			return;
		}
		catch (Transport_rule_list::No_match) { }
		catch (Permit_single_rule_tree::No_match) { }
	}
	catch (Interface::Bad_transport_protocol) { }

	/* try to route via IP rules */
	try {
		Ip_rule const &rule =
			_domain.ip_rules().longest_prefix_match(ip.dst());

		if(_config().verbose()) {
			log("Using IP rule: ", rule); }

		Domain &domain = rule.domain();
		_adapt_eth(eth, eth_size, ip.dst(), pkt, domain);
		domain.interfaces().for_each([&] (Interface &interface) {
			interface._pass_ip(eth, eth_size, ip);
		});

		return;
	}
	catch (Ip_rule_list::No_match) { }

	/* give up and drop packet */
	if (_config().verbose()) {
		log("Unroutable packet"); }
}


void Interface::_broadcast_arp_request(Ipv4_address const &ip)
{
	enum {
		ETH_HDR_SZ = sizeof(Ethernet_frame),
		ETH_DAT_SZ = sizeof(Arp_packet) + ETH_HDR_SZ >= Ethernet_frame::MIN_SIZE ?
		             sizeof(Arp_packet) :
		             Ethernet_frame::MIN_SIZE - ETH_HDR_SZ,
		ETH_CRC_SZ = sizeof(Genode::uint32_t),
		PKT_SIZE   = ETH_HDR_SZ + ETH_DAT_SZ + ETH_CRC_SZ,
	};
	send(PKT_SIZE, [&] (void *pkt_base) {

		/* write Ethernet header */
		Ethernet_frame &eth = *reinterpret_cast<Ethernet_frame *>(pkt_base);
		eth.dst(Mac_address(0xff));
		eth.src(_router_mac);
		eth.type(Ethernet_frame::Type::ARP);

		/* write ARP header */
		Arp_packet &arp = *eth.data<Arp_packet>();
		arp.hardware_address_type(Arp_packet::ETHERNET);
		arp.protocol_address_type(Arp_packet::IPV4);
		arp.hardware_address_size(sizeof(Mac_address));
		arp.protocol_address_size(sizeof(Ipv4_address));
		arp.opcode(Arp_packet::REQUEST);
		arp.src_mac(_router_mac);
		arp.src_ip(_router_ip());
		arp.dst_mac(Mac_address(0xff));
		arp.dst_ip(ip);
	});
}


void Interface::_handle_arp_reply(Ethernet_frame &eth,
                                  size_t const    eth_size,
                                  Arp_packet     &arp)
{
	try {
		/* check wether a matching ARP cache entry already exists */
		_domain.arp_cache().find_by_ip(arp.src_ip());
		if (_config().verbose()) {
			log("ARP entry already exists"); }
	}
	catch (Arp_cache::No_match) {

		/* by now, no matching ARP cache entry exists, so create one */
		Ipv4_address const ip = arp.src_ip();
		_domain.arp_cache().new_entry(ip, arp.src_mac());

		/* continue handling of packets that waited for the entry */
		for (Arp_waiter_list_element *waiter_le = _domain.foreign_arp_waiters().first();
		     waiter_le; )
		{
			Arp_waiter &waiter = *waiter_le->object();
			waiter_le = waiter_le->next();
			if (ip != waiter.ip()) { continue; }
			waiter.src()._continue_handle_eth(waiter.packet());
			destroy(waiter.src()._alloc, &waiter);
		}
	}
	if (_ip_config().interface.prefix_matches(arp.dst_ip()) &&
	    arp.dst_ip() != _router_ip())
	{
		/*
		 * Packet targets IP local to the domain's subnet and doesn't target
		 * the router. Thus, forward it to all other interfaces of the domain.
		 */
		_domain_broadcast(eth, eth_size);
	}
}


Ipv4_address const &Interface::_router_ip() const
{
	return _ip_config().interface.address;
}


void Interface::_send_arp_reply(Ethernet_frame &eth,
                                size_t const    eth_size,
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
	send(eth, eth_size);
}


void Interface::_handle_arp_request(Ethernet_frame &eth,
                                    size_t const    eth_size,
                                    Arp_packet     &arp)
{
	if (_ip_config().interface.prefix_matches(arp.dst_ip())) {

		/* ARP request for an IP local to the domain's subnet */
		if (arp.src_ip() == arp.dst_ip()) {

			/* gratuitous ARP requests are not really necessary */
			throw Drop_packet_inform("gratuitous ARP request");

		} else if (arp.dst_ip() == _router_ip()) {

			/* ARP request for the routers IP at this domain */
			_send_arp_reply(eth, eth_size, arp);

		} else {

			/* forward request to all other interfaces of the domain */
			_domain_broadcast(eth, eth_size);
		}

	} else {

		/* ARP request for an IP foreign to the domain's subnet */
		if (_ip_config().gateway_valid) {

			/* leave request up to the gateway of the domain */
			throw Drop_packet_inform("leave ARP request up to gateway");

		} else {

			/* try to act as gateway for the domain as none is configured */
			_send_arp_reply(eth, eth_size, arp);
		}
	}
}


void Interface::_handle_arp(Ethernet_frame &eth, size_t const eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t const arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet &arp = *eth.data<Arp_packet>();
	Arp_packet::validate_size(arp_size);
	if (!arp.ethernet_ipv4()) {
		error("ARP for unknown protocol"); }

	switch (arp.opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(eth, eth_size, arp);   break;
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


void Interface::_destroy_dhcp_allocation(Dhcp_allocation &allocation)
{
	_domain.dhcp_server().free_ip(allocation.ip());
	destroy(_alloc, &allocation);
}


void Interface::_destroy_released_dhcp_allocations()
{
	while (Dhcp_allocation *allocation = _released_dhcp_allocations.first()) {
		_released_dhcp_allocations.remove(allocation);
		_destroy_dhcp_allocation(*allocation);
	}
}


void Interface::_handle_eth(void              *const  eth_base,
                            size_t             const  eth_size,
                            Packet_descriptor  const &pkt)
{
	_domain.raise_rx_bytes(eth_size);

	/* do garbage collection over transport-layer links and DHCP allocations */
	_destroy_dissolved_links<Udp_link>(_dissolved_udp_links, _alloc);
	_destroy_dissolved_links<Tcp_link>(_dissolved_tcp_links, _alloc);
	_destroy_released_dhcp_allocations();

	/* inspect and handle ethernet frame */
	try {
		Ethernet_frame *const eth = reinterpret_cast<Ethernet_frame *>(eth_base);
		Ethernet_frame::validate_size(eth_size);
		if (_config().verbose()) {
			log("(router <- ", _domain, ") ", *eth); }

		if (_domain.ip_config().valid) {

			switch (eth->type()) {
			case Ethernet_frame::Type::ARP:  _handle_arp(*eth, eth_size);     break;
			case Ethernet_frame::Type::IPV4: _handle_ip(*eth, eth_size, pkt); break;
			default: throw Bad_network_protocol(); }

		} else {

			switch (eth->type()) {
			case Ethernet_frame::Type::IPV4: _dhcp_client.handle_ip(*eth, eth_size); break;
			default: throw Bad_network_protocol(); }
		}
	}
	catch (Ethernet_frame::No_ethernet_frame) { warning("malformed Ethernet frame"); }
	catch (Ipv4_packet::No_ip_packet)         { warning("malformed IPv4 packet"     ); }
	catch (Tcp_packet::No_tcp_packet)         { warning("malformed TCP packet"    ); }
	catch (Udp_packet::No_udp_packet)         { warning("malformed UDP packet"    ); }
	catch (Dhcp_packet::No_dhcp_packet)       { warning("malformed DHCP packet"   ); }
	catch (Arp_packet::No_arp_packet)         { warning("malformed ARP packet"    ); }

	catch (Bad_network_protocol) {
		if (_config().verbose()) {
			log("unknown network layer protocol");
		}
	}
	catch (Drop_packet_inform exception) {
		if (_config().verbose()) {
			log("(", _domain, ") Drop packet: ", exception.msg);
		}
	}
	catch (Drop_packet_warn exception) {
		warning("(", _domain, ") Drop packet: ", exception.msg);
	}
	catch (Port_allocator_guard::Out_of_indices) {
		error("no available NAT ports"); }

	catch (Domain::No_next_hop) {
		error("can not find next hop"); }

	catch (Alloc_dhcp_msg_buffer_failed) {
		error("failed to allocate buffer for DHCP reply"); }

	catch (Dhcp_msg_buffer_too_small) {
		error("DHCP reply buffer too small"); }

	catch (Dhcp_server::Alloc_ip_failed) {
		error("failed to allocate IP for DHCP client"); }
}


void Interface::send(Ethernet_frame &eth, size_t eth_size)
{
	send(eth_size, [&] (void *pkt_base) {
		Genode::memcpy(pkt_base, (void *)&eth, eth_size);
	});
}


void Interface::_send_alloc_pkt(Packet_descriptor &pkt,
                                void            * &pkt_base,
                                size_t             pkt_size)
{
	pkt      = _source().alloc_packet(pkt_size);
	pkt_base = _source().packet_content(pkt);
}


void Interface::_send_submit_pkt(Packet_descriptor &pkt,
                                 void            * &pkt_base,
                                 size_t             pkt_size)
{
	_source().submit_packet(pkt);
	_domain.raise_tx_bytes(pkt_size);
	if (_config().verbose()) {
		log("(", _domain, " <- router) ",
		    *reinterpret_cast<Ethernet_frame *>(pkt_base));
	}
}


Interface::Interface(Genode::Entrypoint &ep,
                     Timer::Connection  &timer,
                     Mac_address const   router_mac,
                     Genode::Allocator  &alloc,
                     Mac_address const   mac,
                     Domain             &domain)
:
	_sink_ack(ep, *this, &Interface::_ack_avail),
	_sink_submit(ep, *this, &Interface::_ready_to_submit),
	_source_ack(ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail),
	_router_mac(router_mac), _mac(mac), _timer(timer), _alloc(alloc),
	_domain(domain)
{
	_domain.manage_interface(*this);
}


void Interface::_init()
{
	if (!_domain.ip_config().valid) {
		_dhcp_client.discover();
	}
}


void Interface::_ack_packet(Packet_descriptor const &pkt)
{
	if (!_sink().ready_to_ack()) {
		error("ack state FULL");
		return;
	}
	_sink().acknowledge_packet(pkt);
}


void Interface::cancel_arp_waiting(Arp_waiter &waiter)
{
	warning("waiting for ARP cancelled");
	_ack_packet(waiter.packet());
	destroy(_alloc, &waiter);
}


Interface::~Interface()
{
	_domain.dissolve_interface(*this);

	/* destroy our own ARP waiters */
	while (_own_arp_waiters.first()) {
		cancel_arp_waiting(*_own_arp_waiters.first()->object());
	}
	/* destroy links */
	_destroy_links<Tcp_link>(_tcp_links, _dissolved_tcp_links, _alloc);
	_destroy_links<Udp_link>(_udp_links, _dissolved_udp_links, _alloc);

	/* destroy DHCP allocations */
	_destroy_released_dhcp_allocations();
	while (Dhcp_allocation *allocation = _dhcp_allocations.first()) {
		_dhcp_allocations.remove(allocation);
		_destroy_dhcp_allocation(*allocation);
	}
}


Configuration &Interface::_config() const { return _domain.config(); }


Ipv4_config const &Interface::_ip_config() const { return _domain.ip_config(); }
