/*
 * \brief  Fiasco.OC-specific CLI-monitor extensions
 * \author Norman Feske
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
#include <extension.h>
#include <terminal_util.h>
#include <command_line.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
#include <l4/sys/ipc.h>
}


static void clear_host_terminal()
{
	using namespace Fiasco;

	outstring("\e[99S");  /* scroll up */
	outstring("\e[99T");  /* scroll down */
	outstring("\e[199A"); /* move cursor up */
}


struct Kdebug_command : Command
{
	Kdebug_command() : Command("kdebug", "enter kernel debugger (via serial console)") { }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		using namespace Fiasco;

		/* let kernel debugger detect the screen size */
		enter_kdebug("*#JS");

		clear_host_terminal();
		enter_kdebug("Entering kernel debugger... Press [?] for help");
		clear_host_terminal();
	}
};


struct Reboot_command : Command
{
	Reboot_command() : Command("reboot", "reboot machine") { }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		using namespace Fiasco;

		clear_host_terminal();
		enter_kdebug("*#^");
	}
};


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
		Genode::ascii_to(freq, &f, 10);
		tprintf(terminal, "set frequency to %ld Hz\n", f);
		regulator.set_level(f);
	}
};


void init_extension(Command_registry &commands)
{
	try { /* only inserts commands if route to regulator session exists */
		static Regulator::Connection reg(Regulator::CLK_CPU);
		static Cpufreq_command cpufreq_command(reg);
		commands.insert(&cpufreq_command);
	} catch (...) { PDBG("No regulator session available!"); };

	static Kdebug_command kdebug_command;
	commands.insert(&kdebug_command);

	static Reboot_command reboot_command;
	commands.insert(&reboot_command);
}
