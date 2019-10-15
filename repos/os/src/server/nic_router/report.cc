/*
 * \brief  Report generation unit
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <report.h>
#include <xml_node.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


Net::Report::Report(bool        const &verbose,
                    Xml_node    const  node,
                    Timer::Connection &timer,
                    Domain_tree       &domains,
                    Quota       const &shared_quota,
                    Pd_session        &pd,
                    Reporter          &reporter)
:
	_verbose             { verbose },
	_config              { node.attribute_value("config", true) },
	_config_triggers     { node.attribute_value("config_triggers", false) },
	_bytes               { node.attribute_value("bytes", true) },
	_stats               { node.attribute_value("stats", true) },
	_link_state          { node.attribute_value("link_state", false) },
	_link_state_triggers { node.attribute_value("link_state_triggers", false) },
	_quota               { node.attribute_value("quota", true) },
	_shared_quota        { shared_quota },
	_pd                  { pd },
	_reporter            { reporter },
	_domains             { domains },
	_timeout             { timer, *this, &Report::_handle_report_timeout,
	                       read_sec_attr(node, "interval_sec", 5) }
{ }


void Net::Report::_report()
{
	if (!_reporter.enabled()) {
		return;
	}
	try {
		Reporter::Xml_generator xml(_reporter, [&] () {
			if (_quota) {
				xml.node("ram", [&] () {
					xml.attribute("quota",  _pd.ram_quota().value);
					xml.attribute("used",   _pd.used_ram().value);
					xml.attribute("shared", _shared_quota.ram);
				});
				xml.node("cap", [&] () {
					xml.attribute("quota",  _pd.cap_quota().value);
					xml.attribute("used",   _pd.used_caps().value);
					xml.attribute("shared", _shared_quota.cap);
				});
			}
			_domains.for_each([&] (Domain &domain) {
				try { domain.report(xml); }
				catch (Empty) { }
			});
		});
	} catch (Xml_generator::Buffer_exceeded) {
		if (_verbose) {
			log("Failed to generate report"); }
	}
}


void Net::Report::_handle_report_timeout(Duration)
{
	_report();
}


void Net::Report::handle_config()
{
	if (!_config_triggers) {
		return; }

	_report();
}

void Net::Report::handle_link_state()
{
	if (!_link_state_triggers) {
		return; }

	_report();
}
