/*
 * \brief  Test for changing the CPU frequency
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2013-06-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <regulator/consts.h>
#include <regulator_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Main
{
	enum { PERIOD_US = 8 * 1000 * 1000 };

	Env                   &env;
	Timer::Connection      timer         { env };
	Regulator::Connection  cpu_regulator { env, Regulator::CLK_CPU };
	Signal_handler<Main>   timer_handler { env.ep(), *this, &Main::handle_timer };
	bool                   high          { true };

	void handle_timer()
	{
		log("Setting CPU frequency ", high ? "low" : "high");
		cpu_regulator.level(high ? Regulator::CPU_FREQ_200 :
		                           Regulator::CPU_FREQ_1600);
		high = !high;
		timer.trigger_once(PERIOD_US);
	}

	Main(Env &env) : env(env)
	{
		timer.sigh(timer_handler);
		timer.trigger_once(PERIOD_US);
	}
};

void Component::construct(Env &env) { static Main main(env); }
