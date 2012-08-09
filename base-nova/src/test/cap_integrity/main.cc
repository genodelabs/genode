/*
 * \brief  Testing capability integrity
 * \author Christian Prochaska
 * \author Alexander Boettcher
 * \date   2012-02-10
 *
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <log_session/connection.h>

using namespace Genode;

int main(int argc, char **argv)
{
	printf("--- capability integrity test ---\n");
	/* try the first 1000 local name IDs */
	for (unsigned local_name = 0; local_name < 1000; local_name++) {
		Native_capability ram_cap = env()->ram_session_cap();
		Log_session_capability log_session_cap =
			reinterpret_cap_cast<Log_session>(ram_cap);
		Log_session_client log_session_client(log_session_cap);
		try {
			log_session_client.write("test message");
		} catch(...) { }
	}

	printf("--- finished capability integrity test ---\n");
	return 0;
}
