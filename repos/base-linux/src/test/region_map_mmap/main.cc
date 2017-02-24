/*
 * \brief  Linux: Test bug in region_map_mmap.cc
 * \author Christian Helmuth
 * \date   2012-12-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <ram_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

static void test_linux_rmmap_bug(Env &env)
{
	enum { QUOTA = 1*1024*1024, CHUNK = 0x1000, ROUNDS = 0x10 };

	log("line: ", __LINE__);
	Ram_connection ram(env);

#if 1 /* transfer quota */
	log("line: ", __LINE__);
	ram.ref_account(env.ram_session_cap());
	env.ram().transfer_quota(ram.cap(), QUOTA);
#endif

	log("line: ", __LINE__);
	for (unsigned i = 0; i < ROUNDS; ++i) {
		Ram_dataspace_capability ds(ram.alloc(CHUNK));
		log(i + 1, " of ", (unsigned)ROUNDS, " pages allocated");
	}

	log("Done.");
}

struct Main { Main(Env &env); };

Main::Main(Env &env)
{
	Genode::log("--- test-rm_session_mmap started ---");

	test_linux_rmmap_bug(env);
}


void Component::construct(Env &env) { static Main main(env); }
