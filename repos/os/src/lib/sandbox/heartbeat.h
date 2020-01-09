/*
 * \brief  Heartbeat monitoring
 * \author Norman Feske
 * \date   2018-11-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__HEARTBEAT_H_
#define _LIB__SANDBOX__HEARTBEAT_H_

/* local includes */
#include <state_reporter.h>
#include <child_registry.h>
#include <util/noncopyable.h>

namespace Sandbox { class Heartbeat; }


class Sandbox::Heartbeat : Noncopyable
{
	private:

		Env &_env;

		Child_registry &_children;

		Report_update_trigger &_report_update_trigger;

		Constructible<Timer::Connection> _timer { };

		uint64_t _rate_ms = 0;

		Signal_handler<Heartbeat> _timer_handler;

		void _handle_timer()
		{
			bool any_skipped_heartbeats = false;

			_children.for_each_child([&] (Child &child) {

				if (child.skipped_heartbeats())
					any_skipped_heartbeats = true;

				child.heartbeat();
			});

			if (any_skipped_heartbeats)
				_report_update_trigger.trigger_report_update();
		}

	public:

		Heartbeat(Env &env, Child_registry &children,
		          Report_update_trigger &report_update_trigger)
		:
			_env(env), _children(children),
			_report_update_trigger(report_update_trigger),
			_timer_handler(_env.ep(), *this, &Heartbeat::_handle_timer)
		{ }

		void apply_config(Xml_node config)
		{
			bool const enabled = config.has_sub_node("heartbeat");

			_timer.conditional(enabled, _env);

			if (!enabled) {
				_rate_ms = 0;
				return;
			}

			unsigned const rate_ms =
				config.sub_node("heartbeat").attribute_value("rate_ms", 1000UL);

			if (rate_ms != _rate_ms) {
				_rate_ms = rate_ms;
				_timer->sigh(_timer_handler);
				_timer->trigger_periodic(_rate_ms*1000);
			}
		}
};

#endif /* _LIB__SANDBOX__HEARTBEAT_H_ */
