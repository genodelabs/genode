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
	_udp_idle_timeout     (read_sec_attr(node, "udp_idle_timeout_sec",      DEFAULT_UDP_IDLE_TIMEOUT_SEC     )),
	_tcp_idle_timeout     (read_sec_attr(node, "tcp_idle_timeout_sec",      DEFAULT_TCP_IDLE_TIMEOUT_SEC     )),
	_tcp_max_segm_lifetime(read_sec_attr(node, "tcp_max_segm_lifetime_sec", DEFAULT_TCP_MAX_SEGM_LIFETIME_SEC)),
	_node(node)
{
	/* read domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		try { _domains.insert(*new (_alloc) Domain(*this, node, _alloc)); }
		catch (Domain::Invalid) { warning("invalid domain"); }
	});
	/* as they must resolve domain names, create rules after domains */
	_domains.for_each([&] (Domain &domain) {
		if (_verbose) {
			log("Domain: ", domain); }

		domain.create_rules(_domains);
	});
	try {
		/* check whether we shall create a report generator */
		Xml_node const report_node = node.sub_node("report");
		try {
			/* try to re-use existing reporter */
			_reporter.set(legacy._reporter.deref());
			legacy._reporter.unset();
		}
		catch (Pointer<Reporter>::Invalid) {

			/* there is no reporter by now, create a new one */
			_reporter.set(*new (_alloc) Reporter(env, "state"));
		}
		/* create report generator */
		_report.set(*new (_alloc)
			Report(report_node, timer, _domains, _reporter.deref()));
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }
}


Configuration::~Configuration()
{
	/* destroy reporter */
	try { destroy(_alloc, &_reporter.deref()); }
	catch (Pointer<Reporter>::Invalid) { }

	/* destroy report generator */
	try { destroy(_alloc, &_report.deref()); }
	catch (Pointer<Report>::Invalid) { }

	/* destroy domains */
	_domains.destroy_each(_alloc);
}
