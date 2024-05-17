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
	if (_dhcp_server_ptr) {
		if (_dhcp_server_ptr->has_invalid_remote_dns_cfg())
			return false;
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
	ASSERT(_ip_config_dynamic);

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
	_config.with_report([&] (Report &r) { r.handle_config(); });
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


bool Domain::_read_forward_rules(Cstring  const    &protocol,
                                 Domain_dict       &domains,
                                 Xml_node const     node,
                                 char     const    *type,
                                 Forward_rule_tree &rules)
{
	bool result = true;
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		if (!result)
			return;

		Port port = node.attribute_value("port", Port(0));
		if (port == Port(0) || dynamic_port(port)) {
			result = _invalid("invalid forward rule");
			return;
		}
		Ipv4_address to_ip = node.attribute_value("to", Ipv4_address());
		if (!to_ip.valid()) {
			result = _invalid("invalid forward rule");
			return;
		}
		domains.find_by_domain_attr(node,
			[&] (Domain &domain) {
				Forward_rule &rule = *new (_alloc)
					Forward_rule(port, to_ip, node.attribute_value("to_port", Port(0)), domain);
				rules.insert(&rule);
				if (_config.verbose())
					log("[", *this, "] ", protocol, " forward rule: ", rule); },
			[&] { result = _invalid("invalid forward rule"); });
	});
	return result;
}


bool Domain::_invalid(char const *reason) const
{
	if (_config.verbose()) {
		log("[", *this, "] invalid domain (", reason, ")"); }
	return false;
}


bool Domain::_read_transport_rules(Cstring  const      &protocol,
                                   Domain_dict         &domains,
                                   Xml_node const       node,
                                   char     const      *type,
                                   Transport_rule_list &rules)
{
	bool result = true;
	node.for_each_sub_node(type, [&] (Xml_node const node) {
		if (!result)
			return;

		Ipv4_address_prefix dst = node.attribute_value("dst", Ipv4_address_prefix());
		if (!dst.valid()) {
			result = _invalid("invalid transport rule");
			return;
		}
		Transport_rule &rule = *new (_alloc) Transport_rule(dst, _alloc);
		if (!rule.finish_construction(domains, node, protocol, _config, *this)) {
			destroy(_alloc, &rule);
			result = _invalid("invalid transport rule");
			return;
		}
		rules.insert(rule);
		if (_config.verbose())
			log("[", *this, "] ", protocol, " rule: ", rule);
	});
	return result;
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
}


bool Domain::finish_construction() const
{
	if (Domain::name() == Domain_name())
		return _invalid("missing name attribute");

	if (_config.verbose_domain_state()) {
		log("[", *this, "] NIC sessions: ", _interface_cnt); }

	return true;
}


Link_side_tree &Domain::links(L3_protocol const protocol)
{
	switch (protocol) {
	case L3_protocol::TCP:  return _tcp_links;
	case L3_protocol::UDP:  return _udp_links;
	case L3_protocol::ICMP: return _icmp_links;
	default: ASSERT_NEVER_REACHED; }
}


Domain::~Domain()
{
	deinit();
	_ip_config.destruct();
}


bool Domain::init(Domain_dict &domains)
{
	/* read DHCP server configuration */
	bool result = true;
	_node.with_optional_sub_node("dhcp-server", [&] (Xml_node const &dhcp_server_node) {
		if (_ip_config_dynamic) {
			result = _invalid("DHCP server and client at once");
			return;
		}
		Dhcp_server &dhcp_server = *new (_alloc) Dhcp_server(dhcp_server_node, _alloc);
		if (!dhcp_server.finish_construction(dhcp_server_node, domains, *this, ip_config().interface())) {
			result = _invalid("invalid DHCP server");
			return;
		}
		dhcp_server.with_dns_config_from([&] (Domain &domain) {
			domain.ip_config_dependents().insert(this); });

		_dhcp_server_ptr = &dhcp_server;
		if (_config.verbose()) {
			log("[", *this, "] DHCP server: ", dhcp_server); }
	});
	if (!result)
		return result;

	/* read forward and transport rules */
	if (!_read_forward_rules(tcp_name(), domains, _node, "tcp-forward", _tcp_forward_rules) ||
	    !_read_forward_rules(udp_name(), domains, _node, "udp-forward", _udp_forward_rules) ||
	    !_read_transport_rules(tcp_name(),  domains, _node, "tcp",  _tcp_rules) ||
	    !_read_transport_rules(udp_name(),  domains, _node, "udp",  _udp_rules))
		return false;

	/* read NAT rules */
	_node.for_each_sub_node("nat", [&] (Xml_node const node) {
		if (!result)
			return;

		domains.find_by_domain_attr(node,
			[&] (Domain &domain) {
				Nat_rule &rule = *new (_alloc)
					Nat_rule(domain, _tcp_port_alloc, _udp_port_alloc,
					         _icmp_port_alloc, node, _config.verbose());
				_nat_rules.insert(&rule);
				if (_config.verbose())
					log("[", *this, "] NAT rule: ", rule); },
			[&] { result = _invalid("invalid NAT rule"); });
	});
	if (!result)
		return result;

	/* read ICMP rules */
	_node.for_each_sub_node("icmp", [&] (Xml_node const node) {
		if (!result)
			return;

		Ipv4_address_prefix dst = node.attribute_value("dst", Ipv4_address_prefix());
		if (!dst.valid()) {
			result = _invalid("invalid ICMP rule");
			return;
		}
		domains.find_by_domain_attr(node,
			[&] (Domain &domain) { _icmp_rules.insert(*new (_alloc) Ip_rule(dst, domain)); },
			[&] { result = _invalid("invalid ICMP rule"); });
	});
	/* read IP rules */
	_node.for_each_sub_node("ip", [&] (Xml_node const node) {
		if (!result)
			return;

		Ipv4_address_prefix dst = node.attribute_value("dst", Ipv4_address_prefix());
		if (!dst.valid()) {
			result = _invalid("invalid IP rule");
			return;
		}
		domains.find_by_domain_attr(node,
			[&] (Domain &domain) { _ip_rules.insert(*new (_alloc) Ip_rule(dst, domain)); },
			[&] { result = _invalid("invalid IP rule"); });
	});
	return result;
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
	if (_dhcp_server_ptr) {
		_dhcp_server_ptr->with_dns_config_from([&] (Domain &domain) {
			domain.ip_config_dependents().remove(this); });
		_dhcp_server_ptr = nullptr;
		destroy(_alloc, _dhcp_server_ptr);
	}
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


bool Domain::report_empty(Report const &report_cfg) const
{
	bool bytes = report_cfg.bytes();
	bool cfg = report_cfg.config();
	bool stats = report_cfg.stats() && (
		!_tcp_stats.report_empty() || !_udp_stats.report_empty() ||
		!_icmp_stats.report_empty() || !_arp_stats.report_empty() || _dhcp_stats.report_empty());
	bool fragm_ip = report_cfg.dropped_fragm_ipv4() && _dropped_fragm_ipv4;
	bool interfaces = false;
	_interfaces.for_each([&] (Interface const &interface) {
		if (!interface.report_empty(report_cfg))
			interfaces = true; });

	return !bytes && !cfg && !stats && !fragm_ip && !interfaces;
}


void Domain::report(Xml_generator &xml, Report const &report_cfg) const
{
	xml.attribute("name", name());
	if (report_cfg.bytes()) {
		xml.attribute("rx_bytes", _tx_bytes);
		xml.attribute("tx_bytes", _rx_bytes);
	}
	if (report_cfg.config()) {
		xml.attribute("ipv4", String<19>(ip_config().interface()));
		xml.attribute("gw",   String<16>(ip_config().gateway()));
		ip_config().for_each_dns_server([&] (Dns_server const &dns_server) {
			xml.node("dns", [&] () {
				xml.attribute("ip", String<16>(dns_server.ip())); }); });
		ip_config().dns_domain_name().with_string([&] (Dns_domain_name::String const &str) {
			xml.node("dns-domain", [&] () {
				xml.attribute("name", str); }); });
	}
	if (report_cfg.stats()) {
		if (!_tcp_stats.report_empty())  xml.node("tcp-links",        [&] { _tcp_stats.report(xml);  });
		if (!_udp_stats.report_empty())  xml.node("udp-links",        [&] { _udp_stats.report(xml);  });
		if (!_icmp_stats.report_empty()) xml.node("icmp-links",       [&] { _icmp_stats.report(xml); });
		if (!_arp_stats.report_empty())  xml.node("arp-waiters",      [&] { _arp_stats.report(xml);  });
		if (!_dhcp_stats.report_empty()) xml.node("dhcp-allocations", [&] { _dhcp_stats.report(xml); });
	}
	if (report_cfg.dropped_fragm_ipv4() && _dropped_fragm_ipv4)
		xml.node("dropped-fragm-ipv4", [&] () {
			xml.attribute("value", _dropped_fragm_ipv4); });
	_interfaces.for_each([&] (Interface const &interface) {
		if (!interface.report_empty(report_cfg))
			xml.node("interface", [&] { interface.report(xml, report_cfg); });
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


bool Domain_link_stats::report_empty() const { return !refused_for_ram && !refused_for_ports && !destroyed; }


void Domain_link_stats::report(Genode::Xml_generator &xml) const
{
	if (refused_for_ram)   xml.node("refused_for_ram",   [&] { xml.attribute("value", refused_for_ram); });
	if (refused_for_ports) xml.node("refused_for_ports", [&] { xml.attribute("value", refused_for_ports); });
	if (destroyed)         xml.node("destroyed",         [&] { xml.attribute("value", destroyed); });
}


/*************************
 ** Domain_object_stats **
 *************************/

void
Domain_object_stats::dissolve_interface(Interface_object_stats const &stats)
{
	destroyed += stats.destroyed;
}


bool Domain_object_stats::report_empty() const { return !destroyed; }


void Domain_object_stats::report(Genode::Xml_generator &xml) const
{
	if (destroyed) xml.node("destroyed", [&] { xml.attribute("value", destroyed); });
}
