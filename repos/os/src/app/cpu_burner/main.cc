/*
 * \brief  CPU burner
 * \author Norman Feske
 * \date   2015-06-30
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <timer_session/connection.h>

struct Cpu_burner
{
	Timer::Connection _timer;

	unsigned long _percent = 100;

	void _handle_config(unsigned)
	{
		Genode::config()->reload();

		_percent = 100;
		try {
			Genode::config()->xml_node().attribute("percent").value(&_percent);
		} catch (...) { }
	}

	Genode::Signal_dispatcher<Cpu_burner> _config_dispatcher;

	unsigned _burn_per_iteration = 10;

	void _handle_period(unsigned)
	{
		unsigned long const start_ms = _timer.elapsed_ms();

		unsigned iterations = 0;
		for (;; iterations++) {

			unsigned long const curr_ms = _timer.elapsed_ms();
			unsigned long passed_ms     = curr_ms - start_ms;

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

	Genode::Signal_dispatcher<Cpu_burner> _period_dispatcher;

	Cpu_burner(Genode::Signal_receiver &sig_rec)
	:
		_config_dispatcher(sig_rec, *this, &Cpu_burner::_handle_config),
		_period_dispatcher(sig_rec, *this, &Cpu_burner::_handle_period)
	{
		Genode::config()->sigh(_config_dispatcher);
		_handle_config(0);

		_timer.sigh(_period_dispatcher);
		_timer.trigger_periodic(1000*1000);
	}
};


int main(int argc, char **argv)
{

	static Genode::Signal_receiver sig_rec;

	static Cpu_burner cpu_burner(sig_rec);

	while (1) {

		Genode::Signal signal = sig_rec.wait_for_signal();

		Genode::Signal_dispatcher_base *dispatcher =
			static_cast<Genode::Signal_dispatcher_base *>(signal.context());

		dispatcher->dispatch(signal.num());
	}
}
