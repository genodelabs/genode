/*
 * \brief  Linux: Test bug in region_map_mmap.cc
 * \author Christian Helmuth
 * \date   2012-12-19
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/log.h>
#include <ram_session/connection.h>
#include <timer_session/connection.h>


static void test_linux_rmmap_bug()
{
	enum { QUOTA = 1*1024*1024, CHUNK = 0x1000, ROUNDS = 0x10 };

	using namespace Genode;

	log("line: ", __LINE__);
	Ram_connection ram;

#if 1 /* transfer quota */
	log("line: ", __LINE__);
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), QUOTA);
#endif

	log("line: ", __LINE__);
	for (unsigned i = 0; i < ROUNDS; ++i) {
		Ram_dataspace_capability ds(ram.alloc(CHUNK));
		log(i + 1, " of ", (unsigned)ROUNDS, " pages allocated");
	}

	log("Done.");
}


int main()
{
	Genode::log("--- test-rm_session_mmap started ---");

	test_linux_rmmap_bug();
}
