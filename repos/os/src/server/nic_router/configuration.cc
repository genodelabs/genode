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
	_node                           { node }
{ }


void Configuration::_invalid_nic_client(Nic_client &nic_client,
                                        char const *reason)
{
	if (_verbose) {
		log("[", nic_client.domain(), "] invalid NIC client: ", nic_client, " (", reason, ")"); }

	_nic_clients.remove(nic_client);
	destroy(_alloc, &nic_client);
}


void Configuration::_invalid_domain(Domain     &domain,
                                    char const *reason)
{
	if (_verbose) {
		log("[", domain, "] invalid domain (", reason, ") "); }

	_domains.remove(domain);
	destroy(_alloc, &domain);
}


Icmp_packet::Code
Configuration::_init_icmp_type_3_code_on_fragm_ipv4(Xml_node const &node) const
{
	char const *const attr_name { "icmp_type_3_code_on_fragm_ipv4" };
	try {
		Xml_attribute const &attr { node.attribute(attr_name) };
		if (attr.has_value("no")) {
			return Icmp_packet::Code::INVALID;
		}
		uint8_t attr_val { };
		bool const attr_transl_succeeded { attr.value<uint8_t>(attr_val) };
		Icmp_packet::Code const result {
			Icmp_packet::code_from_uint8(
				Icmp_packet::Type::DST_UNREACHABLE, attr_val) };

		if (!attr_transl_succeeded ||
		    result == Icmp_packet::Code::INVALID) {

			warning("attribute 'icmp_type_3_code_on_fragm_ipv4' has invalid "
			        "value, assuming value \"no\"");

			return Icmp_packet::Code::INVALID;
		}
		return result;
	}
	catch (Xml_node::Nonexistent_attribute) {
		return Icmp_packet::Code::INVALID;
	}
}


Configuration::Configuration(Env               &env,
                             Xml_node    const  node,
                             Allocator         &alloc,
                             Cached_timer      &timer,
                             Configuration     &old_config,
                             Quota       const &shared_quota,
                             Interface_list    &interfaces)
:
	_alloc                          { alloc },
	_max_packets_per_signal         { node.attribute_value("max_packets_per_signal",    (unsigned long)150) },
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
	_tcp_max_segm_lifetime          { read_sec_attr(node,  "tcp_max_segm_lifetime_sec", 30) },
	_node                           { node }
{
	/* do parts of domain initialization that do not lookup other domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		try {
			Domain &domain = *new (_alloc) Domain(*this, node, _alloc);
			_domains.insert(
				domain,
				[&] /* handle_name_not_unique */ (Domain &other_domain)
				{
					_invalid_domain(domain,       "name not unique");
					_invalid_domain(other_domain, "name not unique");
				}
			);
		}
		catch (Domain::Invalid) { }
	});
	/* do parts of domain initialization that may lookup other domains */
	while (true) {

		struct Retry_without_domain : Genode::Exception
		{
			Domain &domain;

			Retry_without_domain(Domain &domain) : domain(domain) { }
		};
		try {
			_domains.for_each([&] (Domain &domain) {
				try { domain.init(_domains); }
				catch (Domain::Invalid) { throw Retry_without_domain(domain); }
				if (_verbose) {
					log("[", domain, "] initiated domain"); }
			});
		}
		catch (Retry_without_domain exception) {

			/* destroy domain that became invalid during initialization */
			_domains.remove(exception.domain);
			destroy(_alloc, &exception.domain);

			/* deinitialize the remaining domains again */
			_domains.for_each([&] (Domain &domain) {
				domain.deinit();
				if (_verbose) {
					log("[", domain, "] deinitiated domain"); }
			});
			/* retry to initialize the remaining domains */
			continue;
		}
		break;
	}
	try {
		/* check whether we shall create a report generator */
		Xml_node const report_node = node.sub_node("report");
		try {
			/* try to re-use existing reporter */
			_reporter = old_config._reporter();
			old_config._reporter = Pointer<Reporter>();
		}
		catch (Pointer<Reporter>::Invalid) {

			/* there is no reporter by now, create a new one */
			_reporter = *new (_alloc) Reporter(env, "state", nullptr, 4096 * 4);
		}
		/* create report generator */
		_report = *new (_alloc)
			Report(_verbose, report_node, timer, _domains, shared_quota,
			       env.pd(), _reporter());
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }

	/* initialize NIC clients */
	_node.for_each_sub_node("nic-client", [&] (Xml_node const node) {
		try {
			Nic_client &nic_client = *new (_alloc)
				Nic_client { node, alloc, old_config._nic_clients, env, timer,
				             interfaces, *this };

			_nic_clients.insert(
				nic_client,
				[&] /* handle_name_not_unique */ (Nic_client &other_nic_client)
				{
					_invalid_nic_client(nic_client,       "label not unique");
					_invalid_nic_client(other_nic_client, "label not unique");
				}
			);
		}
		catch (Nic_client::Invalid) { }
	});
	/*
	 * Destroy old NIC clients to ensure that NIC client interfaces that were not
	 * re-used are not re-attached to the new domains.
	 */
	old_config._nic_clients.destroy_each(_alloc);
}


void Configuration::stop_reporting()
{
	if (!_reporter.valid()) {
		return;
	}
	_reporter().enabled(false);
}


void Configuration::start_reporting()
{
	if (!_reporter.valid()) {
		return;
	}
	_reporter().enabled(true);
}


Configuration::~Configuration()
{
	/* destroy NIC clients */
	_nic_clients.destroy_each(_alloc);

	/* destroy reporter */
	try { destroy(_alloc, &_reporter()); }
	catch (Pointer<Reporter>::Invalid) { }

	/* destroy report generator */
	try { destroy(_alloc, &_report()); }
	catch (Pointer<Report>::Invalid) { }

	/* destroy domains */
	_domains.destroy_each(_alloc);
}
