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
	_alloc(alloc),
	_node(node)
{ }


Configuration::Configuration(Env               &env,
                             Xml_node const     node,
                             Allocator         &alloc,
                             Timer::Connection &timer,
                             Configuration     &legacy)
:
	_alloc(alloc),
	_verbose              (node.attribute_value("verbose",              false)),
	_verbose_packets      (node.attribute_value("verbose_packets",      false)),
	_verbose_domain_state (node.attribute_value("verbose_domain_state", false)),
	_dhcp_discover_timeout(read_sec_attr(node, "dhcp_discover_timeout_sec", DEFAULT_DHCP_DISCOVER_TIMEOUT_SEC)),
	_dhcp_request_timeout (read_sec_attr(node, "dhcp_request_timeout_sec",  DEFAULT_DHCP_REQUEST_TIMEOUT_SEC )),
	_dhcp_offer_timeout   (read_sec_attr(node, "dhcp_offer_timeout_sec",    DEFAULT_DHCP_OFFER_TIMEOUT_SEC   )),
	_icmp_idle_timeout    (read_sec_attr(node, "icmp_idle_timeout_sec",     DEFAULT_ICMP_IDLE_TIMEOUT_SEC    )),
	_udp_idle_timeout     (read_sec_attr(node, "udp_idle_timeout_sec",      DEFAULT_UDP_IDLE_TIMEOUT_SEC     )),
	_tcp_idle_timeout     (read_sec_attr(node, "tcp_idle_timeout_sec",      DEFAULT_TCP_IDLE_TIMEOUT_SEC     )),
	_tcp_max_segm_lifetime(read_sec_attr(node, "tcp_max_segm_lifetime_sec", DEFAULT_TCP_MAX_SEGM_LIFETIME_SEC)),
	_node(node)
{
	/* read domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		try { _domains.insert(*new (_alloc) Domain(*this, node, _alloc)); }
		catch (Domain::Invalid) { log("invalid domain"); }
	});
	/* do those parts of domain init that require the domain tree to be complete */
	List<Domain> invalid_domains;
	_domains.for_each([&] (Domain &domain) {
		try {
			domain.init(_domains);
			if (_verbose) {
				log("[", domain, "] initiated domain"); }
		}
		catch (Domain::Invalid) { invalid_domains.insert(&domain); }
	});
	invalid_domains.for_each([&] (Domain &domain) {
		_domains.remove(domain);
		destroy(_alloc, &domain);
	});
	try {
		/* check whether we shall create a report generator */
		Xml_node const report_node = node.sub_node("report");
		try {
			/* try to re-use existing reporter */
			_reporter = legacy._reporter();
			legacy._reporter = Pointer<Reporter>();
		}
		catch (Pointer<Reporter>::Invalid) {

			/* there is no reporter by now, create a new one */
			_reporter = *new (_alloc) Reporter(env, "state");
		}
		/* create report generator */
		_report = *new (_alloc)
			Report(report_node, timer, _domains, _reporter());
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }
}


Configuration::~Configuration()
{
	/* destroy reporter */
	try { destroy(_alloc, &_reporter()); }
	catch (Pointer<Reporter>::Invalid) { }

	/* destroy report generator */
	try { destroy(_alloc, &_report()); }
	catch (Pointer<Report>::Invalid) { }

	/* destroy domains */
	_domains.destroy_each(_alloc);
}
