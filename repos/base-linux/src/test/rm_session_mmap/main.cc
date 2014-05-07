/*
 * \brief  Linux: Test bug in rm_session_mmap.cc
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
#include <base/printf.h>
#include <ram_session/connection.h>
#include <timer_session/connection.h>


static void test_linux_rmmap_bug()
{
	enum { QUOTA = 1*1024*1024, CHUNK = 0x1000, ROUNDS = 0x10 };

	using namespace Genode;

	PLOG("line: %d", __LINE__);
	Ram_connection ram;

#if 1 /* transfer quota */
	PLOG("line: %d", __LINE__);
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), QUOTA);
#endif

	PLOG("line: %d", __LINE__);
	for (unsigned i = 0; i < ROUNDS; ++i) {
		Ram_dataspace_capability ds(ram.alloc(CHUNK));
		PLOG("%d of %d pages allocated", (i + 1), ROUNDS);
	}

	PLOG("Done.");
}


int main()
{
	Genode::printf("--- test-rm_session_mmap started ---\n");

//	Timer::Connection timer;
//	timer.msleep(1000);

	test_linux_rmmap_bug();
}
