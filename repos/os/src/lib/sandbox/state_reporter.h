/*
 * \brief  State reporting mechanism
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__STATE_REPORTER_H_
#define _LIB__SANDBOX__STATE_REPORTER_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <os/sandbox.h>

/* local includes */
#include "report.h"

namespace Sandbox { class State_reporter; }


class Sandbox::State_reporter : public Report_update_trigger
{
	public:

		struct Producer : Interface
		{
			virtual void produce_state_report(Xml_generator &xml,
			                                  Report_detail const &) const = 0;
		};

	private:

		using State_handler = Genode::Sandbox::State_handler;

		Env &_env;

		Producer &_producer;

		Reconstructible<Report_detail> _report_detail { };

		uint64_t _report_delay_ms = 0;

		/* interval used when child-ram reporting is enabled */
		uint64_t _report_period_ms = 0;

		/* version string from config, to be reflected in the report */
		typedef String<64> Version;
		Version _version { };

		Constructible<Timer::Connection> _timer          { };
		Constructible<Timer::Connection> _timer_periodic { };

		Signal_handler<State_reporter> _timer_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		Signal_handler<State_reporter> _timer_periodic_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		Signal_handler<State_reporter> _immediate_handler {
			_env.ep(), *this, &State_reporter::_handle_timer };

		bool _scheduled = false;

		State_handler &_state_handler;

		void _handle_timer()
		{
			_scheduled = false;

			_state_handler.handle_sandbox_state();
		}

	public:

		State_reporter(Env &env, Producer &producer, State_handler &state_handler)
		:
			_env(env), _producer(producer),
			_state_handler(state_handler)
		{ }

		void generate(Xml_generator &xml) const
		{
			if (_version.valid())
				xml.attribute("version", _version);

			if (_report_detail.constructed())
				_producer.produce_state_report(xml, *_report_detail);
		}

		void apply_config(Xml_node config)
		{
			try {
				Xml_node report = config.sub_node("report");

				_report_detail.construct(report);
				_report_delay_ms = report.attribute_value("delay_ms", 100UL);
			}
			catch (Xml_node::Nonexistent_sub_node) {
				_report_detail.construct();
				_report_delay_ms = 0;
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
			 * RAM session without any interplay with the sandbox. The periodic
			 * reports ensure that such changes are reflected by the sandbox'
			 * state report.
			 *
			 * By default, the interval is one second. However, when the
			 * 'delay_ms' attribute is defined with a higher value than that,
			 * the user intends to limit the rate of state reports. If so, we
			 * use the value of 'delay_ms' as interval.
			 */
			uint64_t const period_ms           = max(1000U, _report_delay_ms);
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

		void trigger_immediate_report_update() override
		{
			if (_report_delay_ms)
				Signal_transmitter(_immediate_handler).submit();
		}
};

#endif /* _LIB__SANDBOX__STATE_REPORTER_H_ */
