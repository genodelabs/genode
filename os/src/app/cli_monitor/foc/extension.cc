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

/* local includes */
#include <extension.h>
#include <terminal_util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/kdebug.h>
#include <l4/sys/ipc.h>
}


struct Kdebug_command : Command
{
	Kdebug_command() : Command("kdebug", "enter kernel debugger (via serial console)") { }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		tprintf(terminal, "  Entering kernel debugger...\n");
		tprintf(terminal, "  Press [g] to continue execution.\n");

		using namespace Fiasco;

		/*
		 * Wait a bit to give the terminal a chance to print the usage
		 * information before the kernel debugger takes over.
		 */
		l4_ipc_sleep(l4_timeout(L4_IPC_TIMEOUT_NEVER, l4_timeout_rel(244, 11)));

		enter_kdebug("");

		tprintf(terminal, "\n");
	}
};


void init_extension(Command_registry &commands)
{
	static Kdebug_command kdebug_command;
	commands.insert(&kdebug_command);
}
