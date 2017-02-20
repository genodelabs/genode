/*
 * \brief  Test if global static constructors in hybrid applications get called
 * \author Christian Prochaska
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>

/* local includes */
#include "testlib.h"

/* Linux includes */
#include <stdlib.h>

using namespace Genode;


struct Testapp_testclass
{
	Testapp_testclass()
	{
		Genode::log("Global static constructor of Genode application called");
	}

	void dummy() { }
};


extern Testlib_testclass testlib_testobject;

Testapp_testclass testapp_testobject;


static int exit_status;
static void exit_on_suspended() { exit(exit_status); }


/*
 * Component implements classical main function in construct.
 */
void Component::construct(Genode::Env &env)
{
	log("--- lx_hybrid global static constructor test ---");

	/*
	 * Call a dummy function on each test object to make sure that the
	   objects don't get optimized out.
	 */
	testlib_testobject.dummy();
	testapp_testobject.dummy();

	log("--- returning from main ---");
	exit_status = 0;
	env.ep().schedule_suspend(exit_on_suspended, nullptr);
}
