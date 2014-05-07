/*
 * \brief  Yield command
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _YIELD_COMMAND_H_
#define _YIELD_COMMAND_H_

/* local includes */
#include <child_registry.h>
#include <process_arg_registry.h>

struct Yield_command : Command
{
	Child_registry       &_children;
	Process_arg_registry &_process_args;

	Yield_command(Child_registry &children, Process_arg_registry &process_args)
	:
		Command("yield", "instruct subsystem to yield resources"),
		_children(children),
		_process_args(process_args)
	{
		add_parameter(new Parameter("--ram", Parameter::NUMBER,  "RAM quota to free"));
		add_parameter(new Parameter("--greedy", Parameter::VOID, "withdraw yielded RAM quota"));
	}

	void execute(Command_line &cmd, Terminal::Session &terminal)
	{
		char label[128];
		label[0] = 0;
		if (cmd.argument(0, label, sizeof(label)) == false) {
			tprintf(terminal, "Error: no subsystem name specified\n");
			return;
		}

		Genode::Number_of_bytes ram = 0;
		cmd.parameter("--ram", ram);

		bool const greedy = cmd.parameter_exists("--greedy");

		/* lookup child by its unique name */
		Child *child = _children.first();
		for (; child; child = child->next())
			if (strcmp(child->name(), label) == 0)
				break;

		if (!child) {
			tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
			return;
		}

		child->yield(ram, greedy);

		tprintf(terminal, "requesting '%s' to yield ", child->name());
		tprint_bytes(terminal, ram);
		tprintf(terminal, "\n");
	}

	List<Argument> &arguments() { return _process_args.list; }
};

#endif /* _YIELD_COMMAND_H_ */
