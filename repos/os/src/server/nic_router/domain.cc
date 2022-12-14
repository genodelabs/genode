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


/************
 ** Domain **
 ************/

void Domain::_log_ip_config() const
{
	if (_config.verbose_domain_state()) {
		log("[", *this, "] ", _ip_config_dynamic ? "dynamic" : "static",
		    " IP config: ", *_ip_config);
	}
}


bool Domain::ready() const
{
	if (_dhcp_server.valid()) {
		if (_dhcp_server().has_invalid_remote_dns_cfg()) {
			return false;
		}
	}
	return true;
}


void Domain::update_ready_state()
{
	bool const rdy { ready() };
	_interfaces.for_each([&] (Interface &interface) {
		interface.handle_domain_ready_state(rdy);
	});
}


void Domain::_prepare_reconstructing_ip_config()
{
	if (!_ip_config_dynamic) {
		throw Ip_config_static(); }

	/* discard old IP config if any */
	if (ip_config().valid()) {

		/* mark IP config invalid */
		_ip_config.construct(_alloc);

		/* detach all dependent interfaces from old IP config */
		_interfaces.for_each([&] (Interface &interface) {
			interface.detach_from_ip_config(*this);
		});
		_ip_config_dependents.for_each([&] (Domain &domain) {
			domain.update_ready_state();
		});
		/* dissolve foreign ARP waiters */
		while (_foreign_arp_waiters.first()) {
			Arp_waiter &waiter = *_foreign_arp_waiters.first()->object();
			waiter.src().cancel_arp_waiting(waiter);
		}
		/*
		 * Destroy all link states
		 *
		 * Strictly speaking, it is not necessary to destroy all link states,
		 * only those that this domain applies NAT to. However, the Genode AVL
		 * tree is not built for removing a selection of nodes and trying to do
		 * it anyways is complicated. So, for now, we simply destroy all links.
		 */
		while (Link_side *link_side = _icmp_links.first()) {
			Link &link { link_side->link() };
			link.client_interface().destroy_link(link);
		}
		while (Link_side *link_side = _tcp_links.first()) {
			Link &link { link_side->link() };
			link.client_interface().destroy_link(link);
		}
		while (Link_side *link_side = _udp_links.first()) {
			Link &link { link_side->link() };
			link.client_interface().destroy_link(link);
		}
	}
}


void Domain::_finish_reconstructing_ip_config()
{
	_log_ip_config();

	/* attach all dependent interfaces to new IP config if it is valid */
	if (ip_config().valid()) {
		_interfaces.for_each([&] (Interface &interface) {
			interface.attach_to_ip_config(*this, ip_config());
		});
		_ip_config_dependents.for_each([&] (Domain &domain) {
			domain.update_ready_state();
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
	_reconstruct_ip_config([&] (Reconstructible<Ipv4_config> &ip_config) {
		ip_config.construct(_alloc); });
}


void Domain::ip_config_from_dhcp_ack(Dhcp_packet &dhcp_ack)
{
	/*
	 * If the IP config didn't change (a common case on DHCP RENEW), prevent
	 * detaching from the old config and attaching to the new one. Because this
	 * would not only create unnecessary CPU overhead but also force all
	 * clients at all interfaces that are listening to this config (via config
	 * attribute 'dns_config_from') to restart their networking (re-do DHCP).
	 */
	Ipv4_config const new_ip_config { dhcp_ack, _alloc, *this };
	if (*_ip_config == new_ip_config) {
		return;
	}
	_reconstruct_ip_config([&] (Reconstructible<Ipv4_config> &ip_config) {
		ip_config.construct(new_ip_config); });
}


void Domain::try_reuse_ip_config(Domain const &domain)
{
	if (ip_config().valid() ||
	    !_ip_config_dynamic ||
	    !domain.ip_config().valid() ||
	    !domain._ip_config_dynamic)
	{
		return;
	}
	_reconstruct_ip_config([&] (Reconstructible<Ipv4_config> &ip_config) {
		ip_config.construct(domain.ip_config(), _alloc); });
}


void Domain::_read_forward_rules(Cstring  const    &protocol,
                                 Domain_dict       &domains,
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
                                   Domain_dict         &domains,
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
	if (name() == Domain_name()) {
		Genode::print(output, "?");
	} else {
		Genode::print(output, name()); }
}


Domain::Domain(Configuration     &config,
               Xml_node    const &node,
               Domain_name const &name,
               Allocator         &alloc,
               Domain_dict       &domains)
:
	Domain_dict::Element { domains, name },
	_config              { config },
	_node                { node },
	_alloc               { alloc },
	_ip_config           { node, alloc },
	_verbose_packets     { node.attribute_value("verbose_packets",
	                                            config.verbose_packets()) },
	_verbose_packet_drop { node.attribute_value("verbose_packet_drop",
	                                            config.verbose_packet_drop()) },
	_trace_packets       { node.attribute_value("trace_packets",
	                                            config.trace_packets()) },
	_icmp_echo_server    { node.attribute_value("icmp_echo_server",
	                                            config.icmp_echo_server()) },
	_use_arp             { _node.attribute_value("use_arp", true) },
	_label               { node.attribute_value("label",
	                                            String<160>()).string() }
{
	_log_ip_config();

	if (Domain::name() == Domain_name()) {
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
	if (dhcp_server.has_invalid_remote_dns_cfg()) {
		throw Pointer<Dhcp_server>::Invalid();
	}
	return dhcp_server;
}


void Domain::init(Domain_dict &domains)
{
	/* read DHCP server configuration */
	try {
		Xml_node const dhcp_server_node = _node.sub_node("dhcp-server");
		if (_ip_config_dynamic) {
			_invalid("DHCP server and client at once"); }

		try {
			Dhcp_server &dhcp_server = *new (_alloc)
				Dhcp_server(dhcp_server_node, *this, _alloc,
				            ip_config().interface(), domains);

			try {
				dhcp_server.
					dns_config_from().ip_config_dependents().insert(this);
			}
			catch (Pointer<Domain>::Invalid) { }

			_dhcp_server = dhcp_server;
			if (_config.verbose()) {
				log("[", *this, "] DHCP server: ", _dhcp_server()); }
		}
		catch (Bit_allocator_dynamic::Out_of_indices) {

			/*
			 * This message is printed independent from the routers
			 * verbosity configuration in order to track down an exception
			 * of type Bit_allocator_dynamic::Out_of_indices that was
			 * previously not caught. We have observed this exception once,
			 * but without a specific use pattern that would
			 * enable for a systematic reproduction of the issue.
			 * The uncaught exception was observed in a 21.03 Sculpt OS
			 * with a manually configured router, re-configuration involved.
			 */
			log("[", *this, "] DHCP server: failed to initialize ",
			    "(IP range: first ",
			    dhcp_server_node.attribute_value("ip_first", Ipv4_address()),
			    " last ",
			    dhcp_server_node.attribute_value("ip_last", Ipv4_address()),
			    ")");

			throw Dhcp_server::Invalid { };
		}
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
				         _icmp_port_alloc, node, _config.verbose());
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
		try { dhcp_server.dns_config_from().ip_config_dependents().remove(this); }
		catch (Pointer<Domain>::Invalid) { }
		destroy(_alloc, &dhcp_server);
	}
	catch (Pointer<Dhcp_server>::Invalid) { }
}


Ipv4_address const &Domain::next_hop(Ipv4_address const &ip) const
{
	if (ip_config().interface().prefix_matches(ip)) { return ip; }
	if (ip_config().gateway_valid()) { return ip_config().gateway(); }
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
	if (!_interface_cnt) {
		_arp_cache.destroy_all_entries();
		if (_ip_config_dynamic) {
			discard_ip_config();
		}
	}
	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt);
	}
}


void Domain::interface_updates_domain_object(Interface &interface)
{
	_interfaces.remove(&interface);
}


void Domain::report(Xml_generator &xml)
{
	xml.node("domain", [&] () {
		bool empty = true;
		xml.attribute("name", name());
		if (_config.report().bytes()) {
			xml.attribute("rx_bytes", _tx_bytes);
			xml.attribute("tx_bytes", _rx_bytes);
			empty = false;
		}
		if (_config.report().config()) {
			xml.attribute("ipv4", String<19>(ip_config().interface()));
			xml.attribute("gw",   String<16>(ip_config().gateway()));
			ip_config().for_each_dns_server([&] (Dns_server const &dns_server) {
				xml.node("dns", [&] () {
					xml.attribute("ip", String<16>(dns_server.ip()));
				});
			});
			ip_config().dns_domain_name().with_string(
				[&] (Dns_domain_name::String const &str)
			{
				xml.node("dns-domain", [&] () {
					xml.attribute("name", str);
				});
			});
			empty = false;
		}
		if (_config.report().stats()) {
			try { xml.node("tcp-links",        [&] () { _tcp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("udp-links",        [&] () { _udp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("icmp-links",       [&] () { _icmp_stats.report(xml); }); empty = false; } catch (Report::Empty) { }
			try { xml.node("arp-waiters",      [&] () { _arp_stats.report(xml);  }); empty = false; } catch (Report::Empty) { }
			try { xml.node("dhcp-allocations", [&] () { _dhcp_stats.report(xml); }); empty = false; } catch (Report::Empty) { }
		}
		if (_config.report().dropped_fragm_ipv4() && _dropped_fragm_ipv4) {
			xml.node("dropped-fragm-ipv4", [&] () {
				xml.attribute("value", _dropped_fragm_ipv4);
			});
			empty = false;
		}
		_interfaces.for_each([&] (Interface &interface) {
			try {
				interface.report(xml);
				empty = false;
			} catch (Report::Empty) { }
		});
		if (empty) {
			throw Report::Empty(); }
	});
}


void Domain::add_dropped_fragm_ipv4(unsigned long dropped_fragm_ipv4)
{
	_dropped_fragm_ipv4 += dropped_fragm_ipv4;
}


/***********************
 ** Domain_link_stats **
 ***********************/

void
Domain_link_stats::dissolve_interface(Interface_link_stats const &stats)
{
	refused_for_ram   += stats.refused_for_ram;
	refused_for_ports += stats.refused_for_ports;
	destroyed         += stats.destroyed;
}


void Domain_link_stats::report(Genode::Xml_generator &xml)
{
	bool empty = true;

	if (refused_for_ram)   { xml.node("refused_for_ram",   [&] () { xml.attribute("value", refused_for_ram); });   empty = false; }
	if (refused_for_ports) { xml.node("refused_for_ports", [&] () { xml.attribute("value", refused_for_ports); }); empty = false; }
	if (destroyed)         { xml.node("destroyed",         [&] () { xml.attribute("value", destroyed); });         empty = false; }

	if (empty) { throw Report::Empty(); }
}


/*************************
 ** Domain_object_stats **
 *************************/

void
Domain_object_stats::dissolve_interface(Interface_object_stats const &stats)
{
	destroyed += stats.destroyed;
}


void Domain_object_stats::report(Genode::Xml_generator &xml)
{
	bool empty = true;
	if (destroyed) { xml.node("destroyed", [&] () { xml.attribute("value", destroyed); }); empty = false; }
	if (empty) { throw Report::Empty(); }
}
