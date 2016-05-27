/*
 * \brief  Test if global static constructors in hybrid applications get called
 * \author Christian Prochaska
 * \date   2011-11-24
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

/* local includes */
#include "testlib.h"

/* Linux includes */
#include <stdlib.h>

using namespace Genode;


struct Testapp_testclass
{
	Testapp_testclass()
	{
		Genode::printf("Global static constructor of Genode application called\n");
	}

	void dummy() { }
};


extern Testlib_testclass testlib_testobject;

Testapp_testclass testapp_testobject;


static int exit_status;
static void exit_on_suspended() { exit(exit_status); }


Genode::size_t Component::stack_size() { return 16*1024*sizeof(long); }


/*
 * Component implements classical main function in construct.
 */
void Component::construct(Genode::Env &env)
{
	printf("--- lx_hybrid global static constructor test ---\n");

	/*
	 * Call a dummy function on each test object to make sure that the
	   objects don't get optimized out.
	 */
	testlib_testobject.dummy();
	testapp_testobject.dummy();

	printf("--- returning from main ---\n");
	exit_status = 0;
	env.ep().schedule_suspend(exit_on_suspended, nullptr);
}
