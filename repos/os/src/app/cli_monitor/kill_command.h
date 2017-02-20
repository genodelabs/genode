/*
 * \brief  Kill command
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KILL_COMMAND_H_
#define _KILL_COMMAND_H_

/* local includes */
#include <child_registry.h>

namespace Cli_monitor { struct Kill_command; }


struct Cli_monitor::Kill_command : Command
{
	Child_registry &_children;

	Genode::Allocator &_alloc;

	Parameter _kill_all_param { "--all", Parameter::VOID, "kill all subsystems" };

	void _destroy_child(Child *child, Terminal::Session &terminal)
	{
		tprintf(terminal, "destroying subsystem '%s'\n", child->name().string());
		_children.remove(child);
		Genode::destroy(_alloc, child);
	}

	Kill_command(Child_registry &children, Genode::Allocator &alloc)
	:
		Command("kill", "destroy subsystem"), _children(children), _alloc(alloc)
	{
		add_parameter(_kill_all_param);
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
			if (child->name() == label) {
				_destroy_child(child, terminal);
				return;
			}
		}

		tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
	}
};

#endif /* _KILL_COMMAND_H_ */
