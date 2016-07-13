/*
 * \brief  Terminal echo program
 * \author Norman Feske
 * \date   2009-10-16
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/signal.h>
#include <terminal_session/connection.h>

using namespace Genode;

static const bool verbose = false;

int main(int, char **)
{
	static Terminal::Connection terminal;

	enum { READ_BUFFER_SIZE = 100 };
	static char read_buffer[READ_BUFFER_SIZE];

	Signal_receiver sig_rec;
	Signal_context  sig_ctx;

	terminal.read_avail_sigh(sig_rec.manage(&sig_ctx));

	static const char *intro_text =
		"--- Terminal echo test started - now you can type characters to be echoed. ---\r\n";
	terminal.write(intro_text, strlen(intro_text) + 1);

	for (;;) {

		sig_rec.wait_for_signal();

		int num_bytes = terminal.read(read_buffer, sizeof(read_buffer));

		if (verbose && (num_bytes > 0))
			log("got ", num_bytes, " bytes");

		for (int i = 0; i < num_bytes; i++) {
			if (read_buffer[i] == '\r') {
				terminal.write("\n", 1);
			}

			terminal.write(&read_buffer[i], 1);
		}
	}

	return 0;
}
