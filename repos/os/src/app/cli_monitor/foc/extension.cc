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
#include <common.h>


void init_extension(Command_registry &commands)
{
	static Kdebug_command kdebug_command;
	commands.insert(&kdebug_command);

	static Reboot_command reboot_command;
	commands.insert(&reboot_command);
}
