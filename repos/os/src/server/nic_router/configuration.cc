/*
 * \brief  Reflects the current router configuration through objects
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
#include <xml_node.h>

/* Genode includes */
#include <base/allocator.h>
#include <base/log.h>

using namespace Net;
using namespace Genode;


/*******************
 ** Configuration **
 *******************/

Configuration::Configuration(Xml_node const  node,
                             Allocator      &alloc)
:
	_alloc                          { alloc },
	_max_packets_per_signal         { 0 },
	_verbose                        { false },
	_verbose_packets                { false },
	_verbose_packet_drop            { false },
	_verbose_domain_state           { false },
	_trace_packets                  { false },
	_icmp_echo_server               { false },
	_icmp_type_3_code_on_fragm_ipv4 { 0 },
	_dhcp_discover_timeout          { 0 },
	_dhcp_request_timeout           { 0 },
	_dhcp_offer_timeout             { 0 },
	_icmp_idle_timeout              { 0 },
	_udp_idle_timeout               { 0 },
	_tcp_idle_timeout               { 0 },
	_tcp_max_segm_lifetime          { 0 },
	_arp_request_timeout            { 0 },
	_node                           { node }
{ }


void Configuration::_invalid_domain(Domain     &domain,
                                    char const *reason)
{
	if (_verbose) {
		log("[", domain, "] invalid domain (", reason, ") "); }

	destroy(_alloc, &domain);
}


Icmp_packet::Code
Configuration::_init_icmp_type_3_code_on_fragm_ipv4(Xml_node const &node) const
{
	using Attribute_string = String<16>;
	Icmp_packet::Code result = Icmp_packet::Code::INVALID;
	Attribute_string attr_str = node.attribute_value("icmp_type_3_code_on_fragm_ipv4", Attribute_string());
	if (attr_str == "no" || attr_str == Attribute_string())
		return result;

	uint8_t attr_u8 { };
	if (Genode::ascii_to(attr_str.string(), attr_u8) == attr_str.length() - 1) {
		result = Icmp_packet::code_from_uint8(Icmp_packet::Type::DST_UNREACHABLE, attr_u8);
		if (result != Icmp_packet::Code::INVALID)
			return result;
	}
	warning("attribute 'icmp_type_3_code_on_fragm_ipv4' has invalid value");
	return result;
}


Configuration::Configuration(Env                             &env,
                             Xml_node                  const  node,
                             Allocator                       &alloc,
                             Signal_context_capability const &report_signal_cap,
                             Cached_timer                    &timer,
                             Configuration                   &old_config,
                             Quota                     const &shared_quota,
                             Interface_list                  &interfaces)
:
	_alloc                          { alloc },
	_max_packets_per_signal         { node.attribute_value("max_packets_per_signal",    (unsigned long)50) },
	_verbose                        { node.attribute_value("verbose",                   false) },
	_verbose_packets                { node.attribute_value("verbose_packets",           false) },
	_verbose_packet_drop            { node.attribute_value("verbose_packet_drop",       false) },
	_verbose_domain_state           { node.attribute_value("verbose_domain_state",      false) },
	_trace_packets                  { node.attribute_value("trace_packets",             false) },
	_icmp_echo_server               { node.attribute_value("icmp_echo_server",          true) },
	_icmp_type_3_code_on_fragm_ipv4 { _init_icmp_type_3_code_on_fragm_ipv4(node) },
	_dhcp_discover_timeout          { read_sec_attr(node,  "dhcp_discover_timeout_sec", 10) },
	_dhcp_request_timeout           { read_sec_attr(node,  "dhcp_request_timeout_sec",  10) },
	_dhcp_offer_timeout             { read_sec_attr(node,  "dhcp_offer_timeout_sec",    10) },
	_icmp_idle_timeout              { read_sec_attr(node,  "icmp_idle_timeout_sec",     10) },
	_udp_idle_timeout               { read_sec_attr(node,  "udp_idle_timeout_sec",      30) },
	_tcp_idle_timeout               { read_sec_attr(node,  "tcp_idle_timeout_sec",      600) },
	_tcp_max_segm_lifetime          { read_sec_attr(node,  "tcp_max_segm_lifetime_sec", 15) },
	_arp_request_timeout            { read_sec_attr(node,  "arp_request_timeout_sec",   10) },
	_node                           { node }
{
	/* do parts of domain initialization that do not lookup other domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		Domain_name const name {
			node.attribute_value("name", Domain_name { }) };

		_domains.with_element(
			name,
			[&] /* match_fn */ (Domain &other_domain)
			{
				if (_verbose) {

					log("[", name,
					    "] invalid domain (name not unique) ");

					log("[", other_domain,
					    "] invalid domain (name not unique) ");
				}
				destroy(_alloc, &other_domain);
			},
			[&] /* no_match_fn */ ()
			{
				Domain &domain = *new (_alloc) Domain { *this, node, name, _alloc, _domains };
				if (!domain.finish_construction())
					destroy(_alloc, &domain);
			}
		);
	});
	/* do parts of domain initialization that may lookup other domains */
	while (true) {
		Domain *invalid_domain_ptr { };
		_domains.for_each([&] (Domain &domain) {
			if (invalid_domain_ptr)
				return;

			if (!domain.init(_domains)) {
				invalid_domain_ptr = &domain;
				return;
			}
			if (_verbose) {
				log("[", domain, "] initiated domain"); }
		});
		if (!invalid_domain_ptr)
			break;

		/* destroy domain that became invalid during initialization */
		destroy(_alloc, invalid_domain_ptr);

		/* deinitialize the remaining domains again */
		_domains.for_each([&] (Domain &domain) {
			domain.deinit();
			if (_verbose) {
				log("[", domain, "] deinitiated domain"); }
		});
	}
	node.with_optional_sub_node("report", [&] (Xml_node const &report_node) {
		if (old_config._reporter_ptr) {
			/* re-use existing reporter */
			_reporter_ptr = old_config._reporter_ptr;
			old_config._reporter_ptr = nullptr;
		} else {
			/* there is no reporter by now, create a new one */
			_reporter_ptr = new (_alloc) Reporter(env, "state", nullptr, 4096 * 4);
		}
		/* create report generator */
		_report.construct(
			_verbose, report_node, timer, _domains, shared_quota, env.pd(),
			*_reporter_ptr, report_signal_cap);
	});
	/* initialize NIC clients */
	_node.for_each_sub_node("nic-client", [&] (Xml_node const node) {
		Session_label const label {
			node.attribute_value("label",  Session_label::String { }) };

		Domain_name const domain {
			node.attribute_value("domain", Domain_name { }) };

		_nic_clients.with_element(
			label,
			[&] /* match */ (Nic_client &nic_client)
			{
				if (_verbose) {

					log("[", domain, "] invalid NIC client: ",
					    label, " (label not unique)");

					log("[", nic_client.domain(), "] invalid NIC client: ",
					    nic_client.label(), " (label not unique)");
				}
				destroy(_alloc, &nic_client);
			},
			[&] /* no_match */ ()
			{
				Nic_client &nic_client = *new (_alloc) Nic_client { label, domain, alloc, _nic_clients, *this };
				if (!nic_client.finish_construction(env, timer, interfaces, old_config._nic_clients))
					destroy(_alloc, &nic_client);
			}
		);
	});
	/*
	 * Destroy old NIC clients to ensure that NIC client interfaces that were
	 * not re-used are not re-attached to the new domains.
	 */
	old_config._nic_clients.destroy_each(_alloc);
}


Configuration::~Configuration()
{
	/* destroy NIC clients */
	_nic_clients.destroy_each(_alloc);

	/* destroy reporter */
	if (_reporter_ptr)
		destroy(_alloc, _reporter_ptr);

	/* destroy domains */
	_domains.destroy_each(_alloc);
}
