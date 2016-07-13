/*
 * \brief  Fiasco.OC on Arndale specific CLI-monitor extensions
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <regulator_session/connection.h>
#include <util/string.h>

/* local includes */
#include <common.h>

struct Cpufreq_command : Command
{
	Regulator::Connection &regulator;

	Cpufreq_command(Regulator::Connection &r)
	: Command("cpu_frequency", "set/show CPU frequency"),
	  regulator(r) { }

	void execute(Command_line &cmd, Terminal::Session &terminal)
	{
		char freq[128];
		freq[0] = 0;
		if (cmd.argument(0, freq, sizeof(freq)) == false) {
			tprintf(terminal, "Current CPU frequency: %ld Hz\n",
			        regulator.level());
			return;
		}

		unsigned long f = 0;
		Genode::ascii_to(freq, f);
		tprintf(terminal, "set frequency to %ld Hz\n", f);
		regulator.level(f);
	}
};


void init_extension(Command_registry &commands)
{
	try { /* only inserts commands if route to regulator session exists */
		static Regulator::Connection reg(Regulator::CLK_CPU);
		static Cpufreq_command cpufreq_command(reg);
		commands.insert(&cpufreq_command);
	} catch (...) { Genode::warning("no regulator session available!"); };

	static Kdebug_command kdebug_command;
	commands.insert(&kdebug_command);

	static Reboot_command reboot_command;
	commands.insert(&reboot_command);
}
