/*
 * \brief  Status command
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _STATUS_COMMAND_H_
#define _STATUS_COMMAND_H_

/* local includes */
#include <table.h>
#include <child_registry.h>

struct Status_command : Command
{
	Child_registry &_children;
	Ram            &_ram;

	Status_command(Ram &ram, Child_registry &children)
	:
		Command("status", "show runtime status"),
		_children(children), _ram(ram)
	{ }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		using Terminal::tprintf;

		Ram::Status const ram_status = _ram.status();

		tprint_status_bytes(terminal, "  RAM quota: ", ram_status.quota);
		tprint_status_bytes(terminal, "       used: ", ram_status.used);
		tprint_status_bytes(terminal, "      avail: ", ram_status.avail);
		tprint_status_bytes(terminal, "   preserve: ", ram_status.preserve);

		tprintf(terminal, "\n");

		struct Child_info
		{
			enum Column { NAME, QUOTA, LIMIT, XFER, USED, AVAIL, STATUS };

			constexpr static size_t num_columns() { return STATUS + 1; }

			char const *name  = 0;
			Child::Ram_status ram_status;

			static char const *label(Column column)
			{
				switch (column) {
				case NAME:   return "process";
				case QUOTA:  return "quota";
				case LIMIT:  return "limit";
				case XFER:   return "xfer";
				case USED:   return "alloc";
				case AVAIL:  return "avail";
				case STATUS: return "status";
				};
				return "";
			}

			size_t len(Column column) const
			{
				switch (column) {
				case NAME:   return strlen(name);
				case QUOTA:  return format_mib(ram_status.quota);
				case LIMIT:
					return ram_status.limit ? format_mib(ram_status.limit) : 0;

				case XFER:   return format_mib(ram_status.xfer);
				case USED:   return format_mib(ram_status.used);
				case AVAIL:  return format_mib(ram_status.avail);
				case STATUS:
					{
						size_t const req = ram_status.req;
						return req ? strlen("req ") + format_mib(req) : 0;
					}
				};
				return 0;
			}

			static bool left_aligned(Column column)
			{
				switch (column) {
				case NAME:   return true;
				case QUOTA:  return false;
				case LIMIT:  return false;
				case XFER:   return false;
				case USED:   return false;
				case AVAIL:  return false;
				case STATUS: return true;
				};
				return false;
			}

			void print_cell(Terminal::Session &terminal, Column column)
			{
				switch (column) {
				case NAME:   tprintf(terminal, "%s", name); break;
				case QUOTA:  tprint_mib(terminal, ram_status.quota); break;
				case LIMIT:

					if (ram_status.limit)
						tprint_mib(terminal, ram_status.limit);
					break;

				case XFER:   tprint_mib(terminal, ram_status.xfer);  break;
				case USED:   tprint_mib(terminal, ram_status.used);  break;
				case AVAIL:  tprint_mib(terminal, ram_status.avail); break;
				case STATUS:
					if (ram_status.req) {
						tprintf(terminal, "req ");
						tprint_mib(terminal, ram_status.req);
					}
					break;
				};
			}

			Child_info() { }
			Child_info(char const *name, Child::Ram_status ram_status)
			:
				name(name), ram_status(ram_status)
			{ }
		};

		/*
		 * Take snapshot of child information.
		 */
		size_t num_children = 0;
		for (Child *c = _children.first(); c; c = c->next())
			num_children++;

		Child_info child_info[num_children];
		unsigned i = 0;
		for (Child *c = _children.first(); c && i < num_children; c = c->next(), i++)
			child_info[i] = Child_info(c->name().string(), c->ram_status());

		/*
		 * Print table
		 */
		if (num_children) {
			Table<Child_info>::print(terminal, child_info, num_children);
			tprintf(terminal, "\n");
		}
	}
};

#endif /* _STATUS_COMMAND_H_ */
