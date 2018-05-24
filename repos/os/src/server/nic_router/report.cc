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


Net::Report::Report(Xml_node const     node,
                    Timer::Connection &timer,
                    Domain_tree       &domains,
                    Reporter          &reporter)
:
	_config(node.attribute_value("config", true)),
	_config_triggers(node.attribute_value("config_triggers", false)),
	_bytes(node.attribute_value("bytes", true)),
	_reporter(reporter),
	_domains(domains),
	_timeout(timer, *this, &Report::_handle_report_timeout,
	         read_sec_attr(node, "interval_sec", 5))
{
	_reporter.enabled(true);
}


void Net::Report::_report()
{
	try {
		Reporter::Xml_generator xml(_reporter, [&] () {
			_domains.for_each([&] (Domain &domain) {
				domain.report(xml);
			});
		});
	} catch (Xml_generator::Buffer_exceeded) {
		Genode::warning("Failed to generate report");
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
