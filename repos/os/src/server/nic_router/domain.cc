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

void Domain::_log_ip_config() const
{
	Ipv4_config const &ip_config = *_ip_config;
	if (_config.verbose()) {
		if (!ip_config.valid &&
			(ip_config.interface_valid ||
			 ip_config.gateway_valid ||
			 ip_config.dns_server_valid))
		{
			log("[", *this, "] malformed ", _ip_config_dynamic ? "dynamic" :
			    "static", "IP config: ", ip_config);
		}
	}
	if (_config.verbose_domain_state()) {
		log("[", *this, "] ", _ip_config_dynamic ? "dynamic" : "static",
		    " IP config: ", ip_config);
	}
}


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
		/* dissolve foreign ARP waiters */
		while (_foreign_arp_waiters.first()) {
			Arp_waiter &waiter = *_foreign_arp_waiters.first()->object();
			waiter.src().cancel_arp_waiting(waiter);
		}
	}
	/* overwrite old with new IP config */
	_ip_config.construct(new_ip_config);
	_log_ip_config();

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
				log("[", *this, "] ", protocol, " forward rule: ", rule); }
		}
		catch (Forward_rule::Invalid) { _invalid("invalid forward rule"); }
	});
}


void Domain::_invalid(char const *reason) const
{
	if (_config.verbose()) {
		log("[", *this, "] invalid domain (", reason, ")"); }
	throw Invalid();
}


void Domain::_read_transport_rules(Cstring  const      &protocol,
                                   Domain_tree         &domains,
                                   Xml_node const       node,
                                   char     const      *type,
                                   Transport_rule_list &rules)
{
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		try {
			rules.insert(*new (_alloc)
				Transport_rule(domains, node, _alloc, protocol, _config, *this));
		}
		catch (Transport_rule::Invalid)     { _invalid("invalid transport rule"); }
		catch (Permit_any_rule::Invalid)    { _invalid("invalid permit-any rule"); }
		catch (Permit_single_rule::Invalid) { _invalid("invalid permit rule"); }
	});
}


void Domain::print(Output &output) const
{
	if (_name == Domain_name()) {
		Genode::print(output, "?");
	} else {
		Genode::print(output, _name); }
}


Domain::Domain(Configuration &config, Xml_node const node, Allocator &alloc)
:
	Domain_base(node), Avl_string_base(_name.string()), _config(config),
	_node(node), _alloc(alloc),
	_ip_config(_node.attribute_value("interface",  Ipv4_address_prefix()),
	           _node.attribute_value("gateway",    Ipv4_address()),
	           Ipv4_address()),
	_verbose_packets(_node.attribute_value("verbose_packets",
	                                       _config.verbose_packets())),
	_verbose_packet_drop(_node.attribute_value("verbose_packet_drop",
	                                           _config.verbose_packet_drop())),
	_icmp_echo_server(_node.attribute_value("icmp_echo_server",
	                                        _config.icmp_echo_server())),
	_label(_node.attribute_value("label", String<160>()).string())
{
	_log_ip_config();

	if (_name == Domain_name()) {
		_invalid("missing name attribute"); }

	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt); }
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
	deinit();
	_ip_config.destruct();
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
			_invalid("DHCP server and client at once"); }

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
	catch (Dhcp_server::Invalid) { _invalid("invalid DHCP server"); }

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
			Nat_rule &rule = *new (_alloc)
				Nat_rule(domains, _tcp_port_alloc, _udp_port_alloc,
				         _icmp_port_alloc, node);
			_nat_rules.insert(&rule);
			if (_config.verbose()) {
				log("[", *this, "] NAT rule: ", rule); }
		}
		catch (Nat_rule::Invalid) { _invalid("invalid NAT rule"); }
	});
	/* read ICMP rules */
	_node.for_each_sub_node("icmp", [&] (Xml_node const node) {
		try { _icmp_rules.insert(*new (_alloc) Ip_rule(domains, node)); }
		catch (Ip_rule::Invalid) { _invalid("invalid ICMP rule"); }
	});
	/* read IP rules */
	_node.for_each_sub_node("ip", [&] (Xml_node const node) {
		try { _ip_rules.insert(*new (_alloc) Ip_rule(domains, node)); }
		catch (Ip_rule::Invalid) { _invalid("invalid IP rule"); }
	});
}


void Domain::deinit()
{
	_ip_rules.destroy_each(_alloc);
	_nat_rules.destroy_each(_alloc);
	_icmp_rules.destroy_each(_alloc);
	_udp_rules.destroy_each(_alloc);
	_tcp_rules.destroy_each(_alloc);
	_udp_forward_rules.destroy_each(_alloc);
	_tcp_forward_rules.destroy_each(_alloc);
	try {
		Dhcp_server &dhcp_server = _dhcp_server();
		_dhcp_server = Pointer<Dhcp_server>();
		try { dhcp_server.dns_server_from().ip_config_dependents().remove(this); }
		catch (Pointer<Domain>::Invalid) { }
		destroy(_alloc, &dhcp_server);
	}
	catch (Pointer<Dhcp_server>::Invalid) { }
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
