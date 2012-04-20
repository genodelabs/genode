/*
 * \brief  Testing the distinction between user and owner of a RAM dataspace
 * \author Martin Stein
 * \date   2012-04-19
 *
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <ram_session/connection.h>
#include <base/printf.h>

using namespace Genode;


int main(int argc, char **argv)
{
	/* Create some RAM sessions */
	printf("Dataspace ownership test\n");
	static Ram_connection ram_1;
	static Ram_connection ram_2;

	/* Allocate dataspace at one of the RAM sessions */
	ram_1.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram_1.cap(), 8*1024);
	Ram_dataspace_capability ds = ram_1.alloc(sizeof(unsigned));

	/* Try to free dataspace at another RAM session */
	ram_2.free(ds);

	/* Check if dataspace was falsely freed */
	try { env()->rm_session()->attach(ds); }
	catch (...) {
		printf("Test ended faulty.\n");
		return -2;
	}

	/* Try to free dataspace at its originating RAM session */
	ram_1.free(ds);

	/* Check if dataspace was freed as expected */
	try { env()->rm_session()->attach(ds); }
	catch (...) {
		printf("Test ended successfully.\n");
		return 0;
	}
	printf("Test ended faulty.\n");
	return -4;
}

