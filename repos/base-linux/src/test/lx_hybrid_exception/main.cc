/*
 * \brief  Test if the exception mechanism works in hybrid applications
 * \author Christian Prochaska
 * \date   2011-11-22
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/printf.h>

/* Linux includes */
#include <stdlib.h>

using namespace Genode;


class Test_exception { };

static int exit_status;
static void exit_on_suspended() { exit(exit_status); }


Genode::size_t Component::stack_size() { return 16*1024*sizeof(long); }


/*
 * Component implements classical main function in construct.
 */
void Component::construct(Genode::Env &env)
{
	printf("--- lx_hybrid exception test ---\n");

	try {
		printf("Throwing Test_exception\n");
		throw Test_exception();
	} catch (Test_exception) {
		printf("Caught Test_exception\n");
	}

	printf("--- returning from main ---\n");
	exit_status = 0;
	env.ep().schedule_suspend(exit_on_suspended, nullptr);
}
