/*
 * \brief  Init-state reporting mechanism
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INIT__STATE_REPORTER_H_
#define _SRC__INIT__STATE_REPORTER_H_

/* Genode includes */
#include <os/reporter.h>
#include <timer_session/connection.h>

/* local includes */
#include "report.h"

namespace Init { class State_reporter; }

class Init::State_reporter : public Report_update_trigger
{
	public:

		struct Producer : Interface
		{
			virtual void produce_state_report(Xml_generator &xml,
			                                  Report_detail const &) const = 0;
		};

	private:

		Env &_env;

		Producer &_producer;

		Constructible<Reporter> _reporter { };

		size_t _buffer_size = 0;

		Reconstructible<Report_detail> _report_detail { };

		unsigned _report_delay_ms = 0;

		/* interval used when child-ram reporting is enabled */
		unsigned _report_period_ms = 0;

		/* version string from config, to be reflected in the report */
		typedef String<64> Version;
		Version _version { };

		Constructible<Timer::Connection> _timer          { };
		Constructible<Timer::Connection> _timer_periodic { };

		Signal_handler<State_reporter> _timer_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		Signal_handler<State_reporter> _timer_periodic_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		bool _scheduled = false;

		void _handle_timer()
		{
			_scheduled = false;

			try {
				Reporter::Xml_generator xml(*_reporter, [&] () {

					if (_version.valid())
						xml.attribute("version", _version);

					_producer.produce_state_report(xml, *_report_detail);
				});
			}
			catch(Xml_generator::Buffer_exceeded) {

				error("state report exceeds maximum size");

				/* try to reflect the error condition as state report */
				try {
					Reporter::Xml_generator xml(*_reporter, [&] () {
						xml.attribute("error", "report buffer exceeded"); });
				}
				catch (...) { }
			}
		}

	public:

		State_reporter(Env &env, Producer &producer)
		:
			_env(env), _producer(producer)
		{ }

		void apply_config(Xml_node config)
		{
			try {
				Xml_node report = config.sub_node("report");

				/* (re-)construct reporter whenever the buffer size is changed */
				Number_of_bytes const buffer_size =
					report.attribute_value("buffer", Number_of_bytes(4096));

				if (buffer_size != _buffer_size || !_reporter.constructed()) {
					_buffer_size = buffer_size;
					_reporter.construct(_env, "state", "state", _buffer_size);
				}

				_report_detail.construct(report);
				_report_delay_ms = report.attribute_value("delay_ms", 100UL);
				_reporter->enabled(true);
			}
			catch (Xml_node::Nonexistent_sub_node) {
				_report_detail.construct();
				_report_delay_ms = 0;
				if (_reporter.constructed())
					_reporter->enabled(false);
			}

			bool trigger_update = false;

			Version const version = config.attribute_value("version", Version());
			if (version != _version) {
				_version = version;
				trigger_update = true;
			}

			if (_report_delay_ms) {
				if (!_timer.constructed()) {
					_timer.construct(_env);
					_timer->sigh(_timer_handler);
				}
				trigger_update = true;
			}

			if (trigger_update)
				trigger_report_update();

			/*
			 * If the report features information about child-RAM quotas, we
			 * update the report periodically. Even in the absence of any other
			 * report-triggering event, a child may consume/free RAM from its
			 * RAM session without any interplay with init. The periodic
			 * reports ensure that such changes are reflected by init's state
			 * report.
			 *
			 * By default, the interval is one second. However, when the
			 * 'delay_ms' attribute is defined with a higher value than that,
			 * the user intends to limit the rate of state reports. If so, we
			 * use the value of 'delay_ms' as interval.
			 */
			unsigned const period_ms           = max(1000U, _report_delay_ms);
			bool     const period_changed      = (_report_period_ms != period_ms);
			bool     const report_periodically = _report_detail->child_ram()
			                                  || _report_detail->child_caps();

			if (report_periodically && !_timer_periodic.constructed()) {
				_timer_periodic.construct(_env);
				_timer_periodic->sigh(_timer_periodic_handler);
			}

			if (!report_periodically && _timer_periodic.constructed()) {
				_report_period_ms = 0;
				_timer_periodic.destruct();
			}

			if (period_changed && _timer_periodic.constructed()) {
				_report_period_ms = period_ms;
				_timer_periodic->trigger_periodic(1000*_report_period_ms);
			}
		}

		void trigger_report_update() override
		{
			if (!_scheduled && _timer.constructed() && _report_delay_ms) {
				_timer->trigger_once(_report_delay_ms*1000);
				_scheduled = true;
			}
		}
};

#endif /* _SRC__INIT__STATE_REPORTER_H_ */
