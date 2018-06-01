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


void Configuration::_invalid_domain(Domain     &domain,
                                    char const *reason)
{
	if (_verbose) {
		log("[", domain, "] invalid domain (", reason, ") "); }

	_domains.remove(domain);
	destroy(_alloc, &domain);
}


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
	/* do parts of domain initialization that do not lookup other domains */
	node.for_each_sub_node("domain", [&] (Xml_node const node) {
		try {
			Domain &domain = *new (_alloc) Domain(*this, node, _alloc);
			try { _domains.insert(domain); }
			catch (Domain_tree::Name_not_unique exception) {
				_invalid_domain(domain,           "name not unique");
				_invalid_domain(exception.object, "name not unique");
			}
		}
		catch (Domain::Invalid) { log("[?] invalid domain"); }
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

			/* deinitialize all domains again */
			_domains.for_each([&] (Domain &domain) {
				domain.deinit();
				if (_verbose) {
					log("[", domain, "] deinitiated domain"); }
			});
			/* destroy domain that became invalid during initialization */
			_domains.remove(exception.domain);
			destroy(_alloc, &exception.domain);

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
