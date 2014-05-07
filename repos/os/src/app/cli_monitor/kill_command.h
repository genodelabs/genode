/*
 * \brief  Kill command
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KILL_COMMAND_H_
#define _KILL_COMMAND_H_

/* local includes */
#include <child_registry.h>
#include <process_arg_registry.h>

struct Kill_command : Command
{
	Child_registry &_children;

	Process_arg_registry &_process_args;

	void _destroy_child(Child *child, Terminal::Session &terminal)
	{
		tprintf(terminal, "destroying subsystem '%s'\n", child->name());
		_process_args.list.remove(&child->argument);
		_children.remove(child);
		Genode::destroy(Genode::env()->heap(), child);
	}

	Kill_command(Child_registry &children, Process_arg_registry &process_args)
	:
		Command("kill", "destroy subsystem"),
		_children(children),
		_process_args(process_args)
	{
		add_parameter(new Parameter("--all", Parameter::VOID, "kill all subsystems"));
	}

	void execute(Command_line &cmd, Terminal::Session &terminal)
	{
		bool const kill_all = cmd.parameter_exists("--all");

		if (kill_all) {
			for (Child *child = _children.first(); child; child = _children.first())
				_destroy_child(child, terminal);
			return;
		}

		char label[128];
		label[0] = 0;
		if (cmd.argument(0, label, sizeof(label)) == false) {
			tprintf(terminal, "Error: no subsystem name specified\n");
			return;
		}

		/* lookup child by its unique name */
		for (Child *child = _children.first(); child; child = child->next()) {
			if (strcmp(child->name(), label) == 0) {
				_destroy_child(child, terminal);
				return;
			}
		}

		tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
	}

	List<Argument> &arguments() { return _process_args.list; }
};

#endif /* _KILL_COMMAND_H_ */
