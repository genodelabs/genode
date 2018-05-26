/*
 * \brief  Reflects an effective domain configuration node
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <configuration.h>
#include <l3_protocol.h>
#include <interface.h>

/* Genode includes */
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;


/***********************
 ** Domain_avl_member **
 ***********************/

Domain_avl_member::Domain_avl_member(Domain_name const &name,
                                     Domain            &domain)
:
	Avl_string_base(name.string()), _domain(domain)
{ }


/*****************
 ** Domain_base **
 *****************/

Domain_base::Domain_base(Xml_node const node)
:
	_name(node.attribute_value("name", Domain_name()))
{ }


/************
 ** Domain **
 ************/

void Domain::ip_config(Ipv4_config const &new_ip_config)
{
	if (!_ip_config_dynamic) {
		throw Ip_config_static(); }

	/* discard old IP config if any */
	if (ip_config().valid) {

		/* mark IP config invalid */
		_ip_config.construct();

		/* detach all dependent interfaces from old IP config */
		_interfaces.for_each([&] (Interface &interface) {
			interface.detach_from_ip_config();
		});
		_ip_config_dependents.for_each([&] (Domain &domain) {
			domain._interfaces.for_each([&] (Interface &interface) {
				interface.detach_from_remote_ip_config();
			});
		});
	}
	/* overwrite old with new IP config */
	_ip_config.construct(new_ip_config);

	/* log the event */
	if (_config.verbose_domain_state()) {
		log("[", *this, "] IP config: ", ip_config()); }

	/* attach all dependent interfaces to new IP config if it is valid */
	if (ip_config().valid) {
		_interfaces.for_each([&] (Interface &interface) {
			interface.attach_to_ip_config(*this, ip_config());
		});
		_ip_config_dependents.for_each([&] (Domain &domain) {
			domain._interfaces.for_each([&] (Interface &interface) {
				interface.attach_to_remote_ip_config();
			});
		});
	} else {
		_interfaces.for_each([&] (Interface &interface) {
			interface.attach_to_domain_finish();
		});
	}
	/* force report if configured */
	try { _config.report().handle_config(); }
	catch (Pointer<Report>::Invalid) { }
}


void Domain::discard_ip_config()
{
	/* install invalid IP config */
	Ipv4_config const new_ip_config;
	ip_config(new_ip_config);
}


void Domain::ip_config(Ipv4_address ip,
                       Ipv4_address subnet_mask,
                       Ipv4_address gateway,
                       Ipv4_address dns_server)
{
	Ipv4_config const new_ip_config(Ipv4_address_prefix(ip, subnet_mask),
	                                gateway, dns_server);
	ip_config(new_ip_config);
}


void Domain::try_reuse_ip_config(Domain const &domain)
{
	if (ip_config().valid ||
	    !_ip_config_dynamic ||
	    !domain.ip_config().valid ||
	    !domain._ip_config_dynamic)
	{
		return;
	}
	ip_config(domain.ip_config());
}


void Domain::_read_forward_rules(Cstring  const    &protocol,
                                 Domain_tree       &domains,
                                 Xml_node const     node,
                                 char     const    *type,
                                 Forward_rule_tree &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		try {
			Forward_rule &rule = *new (_alloc) Forward_rule(domains, node);
			rules.insert(&rule);
			if (_config.verbose()) {
				log("[", *this, "] forward rule: ", protocol, " ", rule); }
		}
		catch (Rule::Invalid) {
			log("[", *this, "] invalid domain (invalid forward rule)");
			throw Invalid();
		}
	});
}


void Domain::_read_transport_rules(Cstring  const      &protocol,
                                   Domain_tree         &domains,
                                   Xml_node const       node,
                                   char     const      *type,
                                   Transport_rule_list &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		try {
			rules.insert(*new (_alloc) Transport_rule(domains, node, _alloc,
			                                          protocol, _config));
		}
		catch (Rule::Invalid) {
			log("[", *this, "] invalid domain (invalid transport rule)");
			throw Invalid();
		}
	});
}


void Domain::print(Output &output) const
{
	Genode::print(output, _name);
}


Domain::Domain(Configuration &config, Xml_node const node, Allocator &alloc)
:
	Domain_base(node), _avl_member(_name, *this), _config(config),
	_node(node), _alloc(alloc),
	_ip_config(_node.attribute_value("interface",  Ipv4_address_prefix()),
	           _node.attribute_value("gateway",    Ipv4_address()),
	           Ipv4_address()),
	_verbose_packets(_node.attribute_value("verbose_packets", false) ||
	                 _config.verbose_packets()),
	_label(_node.attribute_value("label", String<160>()).string())
{
	if (_name == Domain_name()) {
		log("[?] Missing name attribute in domain node");
		throw Invalid();
	}
	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt);
	}
}


Link_side_tree &Domain::links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _tcp_links;
	case L3_protocol::UDP:  return _udp_links;
	case L3_protocol::ICMP: return _icmp_links;
	default: throw Interface::Bad_transport_protocol(); }
}


Domain::~Domain()
{
	/* destroy rules */
	_ip_rules.destroy_each(_alloc);
	_nat_rules.destroy_each(_alloc);
	_icmp_rules.destroy_each(_alloc);
	_udp_rules.destroy_each(_alloc);
	_tcp_rules.destroy_each(_alloc);
	_udp_forward_rules.destroy_each(_alloc);
	_tcp_forward_rules.destroy_each(_alloc);

	/* destroy DHCP server and IP config */
	try { destroy(_alloc, &_dhcp_server()); }
	catch (Pointer<Dhcp_server>::Invalid) { }
	_ip_config.destruct();
}


void Domain::__FIXME__dissolve_foreign_arp_waiters()
{
	/* let other interfaces destroy their ARP waiters that wait for us */
	while (_foreign_arp_waiters.first()) {
		Arp_waiter &waiter = *_foreign_arp_waiters.first()->object();
		waiter.src().cancel_arp_waiting(waiter);
	}
}


Dhcp_server &Domain::dhcp_server()
{
	Dhcp_server &dhcp_server = _dhcp_server();
	if (!dhcp_server.ready()) {
		throw Pointer<Dhcp_server>::Invalid();
	}
	return dhcp_server;
}


void Domain::init(Domain_tree &domains)
{
	/* read DHCP server configuration */
	try {
		Xml_node const dhcp_server_node = _node.sub_node("dhcp-server");
		if (_ip_config_dynamic) {
			log("[", *this, "] invalid domain (DHCP server and client at once)");
			throw Invalid();
		}
		Dhcp_server &dhcp_server = *new (_alloc)
			Dhcp_server(dhcp_server_node, *this, _alloc,
			            ip_config().interface, domains);

		try { dhcp_server.dns_server_from().ip_config_dependents().insert(this); }
		catch (Pointer<Domain>::Invalid) { }

		_dhcp_server = dhcp_server;
		if (_config.verbose()) {
			log("[", *this, "] DHCP server: ", _dhcp_server()); }
	}
	catch (Xml_node::Nonexistent_sub_node) { }
	catch (Dhcp_server::Invalid) {
		if (_config.verbose()) {
			log("[", *this, "] invalid domain (invalid DHCP server)"); }
		throw Invalid();
	}

	/* read forward rules */
	_read_forward_rules(tcp_name(), domains, _node, "tcp-forward",
	                    _tcp_forward_rules);
	_read_forward_rules(udp_name(), domains, _node, "udp-forward",
	                    _udp_forward_rules);

	/* read UDP and TCP rules */
	_read_transport_rules(tcp_name(),  domains, _node, "tcp",  _tcp_rules);
	_read_transport_rules(udp_name(),  domains, _node, "udp",  _udp_rules);

	/* read NAT rules */
	_node.for_each_sub_node("nat", [&] (Xml_node const node) {
		try {
			_nat_rules.insert(
				new (_alloc) Nat_rule(domains, _tcp_port_alloc,
				                      _udp_port_alloc, _icmp_port_alloc,
				                      node));
		}
		catch (Rule::Invalid) {
			log("[", *this, "] invalid domain (invalid NAT rule)");
			throw Invalid();
		}
	});
	/* read ICMP rules */
	_node.for_each_sub_node("icmp", [&] (Xml_node const node) {
		try { _icmp_rules.insert(*new (_alloc) Ip_rule(domains, node)); }
		catch (Rule::Invalid) {
			log("[", *this, "] invalid domain (invalid ICMP rule)");
			throw Invalid();
		}
	});
	/* read IP rules */
	_node.for_each_sub_node("ip", [&] (Xml_node const node) {
		try { _ip_rules.insert(*new (_alloc) Ip_rule(domains, node)); }
		catch (Rule::Invalid) {
			log("[", *this, "] invalid domain (invalid IP rule)");
			throw Invalid();
		}
	});
}


Ipv4_address const &Domain::next_hop(Ipv4_address const &ip) const
{
	if (ip_config().interface.prefix_matches(ip)) { return ip; }
	if (ip_config().gateway_valid) { return ip_config().gateway; }
	throw No_next_hop();
}


void Domain::attach_interface(Interface &interface)
{
	_interfaces.insert(&interface);
	_interface_cnt++;
	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt);
	}
}


void Domain::detach_interface(Interface &interface)
{
	_interfaces.remove(&interface);
	_interface_cnt--;
	if (!_interface_cnt && _ip_config_dynamic) {
		discard_ip_config();
	}
	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt);
	}
}

void Domain::report(Xml_generator &xml)
{
	bool const bytes  = _config.report().bytes();
	bool const config = _config.report().config();
	if (!bytes && !config) {
		return;
	}
	xml.node("domain", [&] () {
		xml.attribute("name", _name);
		if (bytes) {
			xml.attribute("rx_bytes", _tx_bytes);
			xml.attribute("tx_bytes", _rx_bytes);
		}
		if (config) {
			xml.attribute("ipv4", String<19>(ip_config().interface));
			xml.attribute("gw",   String<16>(ip_config().gateway));
			xml.attribute("dns",  String<16>(ip_config().dns_server));
		}
	});
}


/*****************
 ** Domain_tree **
 *****************/

Domain &Domain_tree::domain(Avl_string_base const &node)
{
	return static_cast<Domain_avl_member const *>(&node)->domain();
}


Domain &Domain_tree::find_by_name(Domain_name name)
{
	if (name == Domain_name() || !first()) {
		throw No_match(); }

	Avl_string_base *node = first()->find_by_name(name.string());
	if (!node) {
		throw No_match(); }

	return domain(*node);
}


void Domain_tree::destroy_each(Deallocator &dealloc)
{
	while (Avl_string_base *first_ = first()) {
		Domain &domain_ = domain(*first_);
		Avl_tree::remove(first_);
		destroy(dealloc, &domain_);
	}
}
