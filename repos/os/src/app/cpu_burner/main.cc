/*
 * \brief  CPU burner
 * \author Norman Feske
 * \date   2015-06-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Cpu_burner
{
	Genode::Env &_env;

	Timer::Connection _timer { _env };

	unsigned long _percent = 100;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	void _handle_config()
	{
		_config.update();
		_percent = _config.xml().attribute_value("percent", 100L);
	}

	Genode::Signal_handler<Cpu_burner> _config_handler {
		_env.ep(), *this, &Cpu_burner::_handle_config };

	unsigned _burn_per_iteration = 10;

	void _handle_period()
	{
		uint64_t const start_ms = _timer.elapsed_ms();

		unsigned iterations = 0;
		for (;; iterations++) {

			uint64_t const curr_ms = _timer.elapsed_ms();
			uint64_t passed_ms     = curr_ms - start_ms;

			if (passed_ms >= 10*_percent)
				break;

			/* burn some time */
			for (unsigned volatile i = 0; i < _burn_per_iteration; i++)
				for (unsigned volatile j = 0; j < 1000*1000; j++)
					(void) (i*j);
		}

		/* adjust busy loop duration */
		if (iterations > 10)
			_burn_per_iteration *= 2;

		if (iterations < 5)
			_burn_per_iteration /= 2;
	}

	Genode::Signal_handler<Cpu_burner> _period_handler {
		_env.ep(), *this, &Cpu_burner::_handle_period };

	Cpu_burner(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_period_handler);
		_timer.trigger_periodic(1000*1000);
	}
};


void Component::construct(Genode::Env &env)
{
	static Cpu_burner cpu_burner(env);
}
