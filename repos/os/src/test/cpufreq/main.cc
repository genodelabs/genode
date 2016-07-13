/*
 * \brief  Test for changing the CPU frequency
 * \author Stefan Kalkowski
 * \date   2013-06-14
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/env.h>
#include <base/sleep.h>

#include <regulator/consts.h>
#include <regulator_session/connection.h>
#include <timer_session/connection.h>

int main(int argc, char **argv)
{
	using namespace Genode;

	log("--- test-cpufreq started ---");

	static Timer::Connection     timer;
	static Regulator::Connection cpu_regulator(Regulator::CLK_CPU);
	bool high = true;

	while (true) {
		timer.msleep(10000);
		log("Setting CPU frequency ", high ? "low" : "high");
		cpu_regulator.level(high ? Regulator::CPU_FREQ_200
		                         : Regulator::CPU_FREQ_1600);
		high = !high;
	}

	return 1;
}
