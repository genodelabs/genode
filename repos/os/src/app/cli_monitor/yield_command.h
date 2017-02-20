/*
 * \brief  Yield command
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _YIELD_COMMAND_H_
#define _YIELD_COMMAND_H_

/* local includes */
#include <child_registry.h>

namespace Cli_monitor { struct Yield_command; }


struct Cli_monitor::Yield_command : Command
{
	Child_registry &_children;

	Parameter _ram_param    { "--ram", Parameter::NUMBER,  "RAM quota to free" };
	Parameter _greedy_param { "--greedy", Parameter::VOID, "withdraw yielded RAM quota" };

	Yield_command(Child_registry &children)
	:
		Command("yield", "instruct subsystem to yield resources"),
		_children(children)
	{
		add_parameter(_ram_param);
		add_parameter(_greedy_param);
	}

	void _for_each_argument(Argument_fn const &fn) const override
	{
		auto child_name_fn = [&] (Child_base::Name const &child_name) {
			Argument arg(child_name.string(), "");
			fn(arg);
		};

		_children.for_each_child_name(child_name_fn);
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
			if (child->name() == label)
				break;

		if (!child) {
			tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
			return;
		}

		child->yield(ram, greedy);

		tprintf(terminal, "requesting '%s' to yield ", child->name().string());
		tprint_bytes(terminal, ram);
		tprintf(terminal, "\n");
	}
};

#endif /* _YIELD_COMMAND_H_ */
