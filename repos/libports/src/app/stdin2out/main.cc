/*
 * \brief  Utility for forwarding data to stdout by polling stdin
 * \author Norman Feske
 * \date   2020-03-25
 *
 * This program fulfils the purpose of 'tail -f' for the log view of Sculpt OS.
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <unistd.h>

extern "C" int main(int, char **)
{
	while (true) {

		char buffer[4096] { };

		size_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));

		if (bytes == 0)
			sleep(2);
		else
			write(STDOUT_FILENO, buffer, bytes);
	}
}
