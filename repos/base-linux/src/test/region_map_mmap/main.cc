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
#include <pd_session/connection.h>
#include <timer_session/connection.h>

using namespace Genode;

static void test_linux_rmmap_bug(Env &env)
{
	enum { QUOTA = 1*1024*1024, CHUNK = 0x1000, ROUNDS = 0x10 };

	log("line: ", __LINE__);
	Pd_connection pd(env);

	log("line: ", __LINE__);
	pd.ref_account(env.pd_session_cap());
	env.pd().transfer_quota(pd.cap(), Ram_quota{QUOTA});
	env.pd().transfer_quota(pd.cap(), Cap_quota{30});

	log("line: ", __LINE__);
	for (unsigned i = 0; i < ROUNDS; ++i) {
		Ram_dataspace_capability ds(pd.alloc(CHUNK));
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
