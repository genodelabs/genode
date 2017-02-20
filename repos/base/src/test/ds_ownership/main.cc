/*
 * \brief  Testing the distinction between user and owner of a RAM dataspace
 * \author Martin Stein
 * \author Norman Feske
 * \date   2012-04-19
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <ram_session/connection.h>
#include <base/log.h>
#include <base/component.h>


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	log("--- dataspace ownership test ---");

	static Ram_connection ram_1 { env };
	static Ram_connection ram_2 { env };

	log("allocate dataspace from one RAM session");
	ram_1.ref_account(env.ram_session_cap());
	env.ram().transfer_quota(ram_1.cap(), 8*1024);
	Ram_dataspace_capability ds = ram_1.alloc(sizeof(unsigned));

	log("attempt to free dataspace from foreign RAM session");
	ram_2.free(ds);

	log("try to attach dataspace to see if it still exists");
	env.rm().attach(ds);

	log("attach operation succeeded");

	log("free dataspace from legitimate RAM session");
	size_t const quota_before_free = ram_1.avail();
	ram_1.free(ds);
	size_t const quota_after_free = ram_1.avail();

	if (quota_after_free > quota_before_free)
		log("test succeeded");
	else
		error("test failed");
}

