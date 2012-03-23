/*
 * \brief  Testing capability integrity
 * \author Christian Prochaska
 * \author Stefan Kalkowski
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
using namespace Fiasco;

int main(int argc, char **argv)
{
	printf("--- capability integrity test ---\n");

	enum { COUNT = 1000 };

	Cap_index*       idx = cap_idx_alloc()->alloc(COUNT);
	Native_thread_id tid = env()->ram_session_cap().dst();

	/* try the first 1000 local name IDs */
	for (int local_name = 0; local_name < COUNT; local_name++, idx++) {
		idx->id(local_name);
		l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
                    l4_obj_fpage(tid, 0, L4_FPAGE_RWX),
                    idx->kcap() | L4_ITEM_MAP);

		Log_session_capability log_session_cap =
			reinterpret_cap_cast<Log_session>(Native_capability(idx));
		Log_session_client log_session_client(log_session_cap);
		try {
			log_session_client.write("test message");
		} catch(...) { }
	}

	printf("--- finished capability integrity test ---\n");
	return 0;
}
