/*
 * \brief  RAM command
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RAM_COMMAND_H_
#define _RAM_COMMAND_H_

/* local includes */
#include <child_registry.h>

namespace Cli_monitor { struct Ram_command; }


struct Cli_monitor::Ram_command : Command
{
	Child_registry &_children;
	Ram            &_ram;

	Parameter _quota_param { "--quota", Parameter::NUMBER, "new RAM quota" };
	Parameter _limit_param { "--limit", Parameter::NUMBER, "on-demand quota limit" };

	Ram_command(Child_registry &children, Ram &ram)
	:
		Command("ram", "set RAM quota of subsystem"),
		_children(children), _ram(ram)
	{
		add_parameter(_quota_param);
		add_parameter(_limit_param);
	}

	void _set_quota(Terminal::Session &terminal, Child &child, size_t const new_quota)
	{
		size_t const old_quota = child.ram_status().quota;

		if (new_quota > old_quota) {

			size_t       amount = new_quota - old_quota;
			size_t const avail  = _ram.avail();
			if (amount > avail) {
				tprintf(terminal, "upgrade of '%s' exceeds available quota of ",
				        child.name().string());
				tprint_bytes(terminal, avail);
				tprintf(terminal, "\n");
				amount = avail;
			}

			tprintf(terminal, "upgrading quota of '%s' to ", child.name().string());
			tprint_bytes(terminal, old_quota + amount);
			tprintf(terminal, "\n");

			try {
				child.upgrade_ram_quota(amount); }
			catch (Ram::Transfer_quota_failed) {
				tprintf(terminal, "Error: transfer_quota failed\n"); }

		} if (new_quota < old_quota) {

			size_t       amount = old_quota - new_quota;
			size_t const avail  = child.ram_status().avail;

			if (amount > avail) {
				tprintf(terminal, "withdrawal of ");
				tprint_bytes(terminal, amount);
				tprintf(terminal, " exceeds available quota of ");
				tprint_bytes(terminal, avail);
				tprintf(terminal, "\n");
				amount = avail;
			}

			tprintf(terminal, "depleting quota of '%s' to ", child.name().string());
			tprint_bytes(terminal, old_quota - amount);
			tprintf(terminal, "\n");

			try {
				child.withdraw_ram_quota(amount); }
			catch (Ram::Transfer_quota_failed) {
				tprintf(terminal, "Error: transfer_quota failed\n"); }
		}
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

		/* lookup child by its unique name */
		Child *child = _children.first();
		for (; child; child = child->next())
			if (child->name() == label)
				break;

		if (!child) {
			tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
			return;
		}

		bool const limit_specified = cmd.parameter_exists("--limit");
		Genode::Number_of_bytes limit = 0;
		if (limit_specified) {
			cmd.parameter("--limit", limit);
			child->ram_limit(limit);
		}

		if (cmd.parameter_exists("--quota")) {
			Genode::Number_of_bytes quota = 0;
			cmd.parameter("--quota", quota);
			_set_quota(terminal, *child, quota);
		}
	}
};

#endif /* _RAM_COMMAND_H_ */
