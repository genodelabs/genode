/*
 * \brief  Send terminal output on specified input (like expect)
 * \author Stefan Kalkowski
 * \date   2019-04-03
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <terminal_session/connection.h>

using namespace Genode;

struct Main
{
	enum { MAX_LINE_LENGTH = 512 };

	using Line = String<MAX_LINE_LENGTH>;
	Terminal::Connection terminal;
	Signal_handler<Main> read_avail;
	unsigned             line_idx = 0;
	char                 line[MAX_LINE_LENGTH];
	char                 read_buffer[MAX_LINE_LENGTH];
	Line                 expect {};
	Line                 send {};
	bool                 verbose { false };

	void handle_read_avail()
	{
		unsigned num_bytes = terminal.read(read_buffer, sizeof(read_buffer));

		for (unsigned i = 0; i < num_bytes; i++) {

			/* copy over all characters other than line-end */
			if (read_buffer[i] != '\n' &&
			    read_buffer[i] != '\r') {
				line[line_idx++] = read_buffer[i];
				line[line_idx] = 0;
			}

			/* check for expected line-start string, if found send line */
			if (expect.valid() && expect == line) {
				terminal.write(send.string(), send.length()-1);
				terminal.write("\r\n", 2);
			}

			/* check for line end */
			if (read_buffer[i] == '\n') {
				if (verbose) log(Cstring(line));
				line_idx = 0;
			}
		}
	}

	Main(Env &env) : terminal(env),
	                 read_avail(env.ep(), *this, &Main::handle_read_avail)
	{
		terminal.read_avail_sigh(read_avail);

		try {
			Genode::Attached_rom_dataspace config { env, "config" };

			verbose = config.xml().attribute_value("verbose", false);
			expect  = config.xml().attribute_value("expect", Line());
			send    = config.xml().attribute_value("send",   Line());
		} catch (...) { warning("No config data available"); }
	}
};

void Component::construct(Env &env) { static Main main(env); }
