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
#include <node.h>
#include <domain.h>

using namespace Net;
using namespace Genode;


Net::Report::Report(bool                      const &verbose,
                    Node                      const &node,
                    Cached_timer                    &timer,
                    Domain_dict                     &domains,
                    Quota                     const &shared_quota,
                    Pd_session                      &pd,
                    Reporter                        &reporter,
                    Signal_context_capability const &signal_cap)
:
	_verbose             { verbose },
	_config              { node.attribute_value("config", true) },
	_config_triggers     { node.attribute_value("config_triggers", false) },
	_bytes               { node.attribute_value("bytes", true) },
	_stats               { node.attribute_value("stats", true) },
	_dropped_fragm_ipv4  { node.attribute_value("dropped_fragm_ipv4", false) },
	_link_state          { node.attribute_value("link_state", false) },
	_link_state_triggers { node.attribute_value("link_state_triggers", false) },
	_quota               { node.attribute_value("quota", true) },
	_shared_quota        { shared_quota },
	_pd                  { pd },
	_reporter            { reporter },
	_domains             { domains },
	_timeout             { timer, *this, &Report::_handle_report_timeout,
	                       read_sec_attr(node, "interval_sec", 5) },
	_signal_transmitter  { signal_cap }
{
	_reporter.enabled(true);
}


void Net::Report::generate() const
{
	Reporter::Result const result = _reporter.generate([&] (Generator &g) {
		if (_quota) {
			g.node("ram", [&] {
				g.attribute("quota",  _pd.ram_quota().value);
				g.attribute("used",   _pd.used_ram().value);
				g.attribute("shared", _shared_quota.ram);
			});
			g.node("cap", [&] {
				g.attribute("quota",  _pd.cap_quota().value);
				g.attribute("used",   _pd.used_caps().value);
				g.attribute("shared", _shared_quota.cap);
			});
		}
		_domains.for_each([&] (Domain const &domain) {
			if (!domain.report_empty(*this))
				g.node("domain", [&] { domain.report(g, *this); }); });
	});

	if (result == Buffer_error::EXCEEDED && _verbose)
		warning("report exceeds maximum buffer size");
}


void Net::Report::_handle_report_timeout(Duration)
{
	generate();
}


void Net::Report::handle_config()
{
	if (!_config_triggers) {
		return;
	}
	_signal_transmitter.submit();
}


void Net::Report::handle_interface_link_state()
{
	if (!_link_state_triggers) {
		return;
	}
	_signal_transmitter.submit();
}
