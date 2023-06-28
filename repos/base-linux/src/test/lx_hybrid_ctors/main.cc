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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <stdlib.h>
#include <stdio.h>
#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

using namespace Genode;


struct Testapp_testclass
{
	Testapp_testclass()
	{
		printf("[init -> test-lx_hybrid_ctors] Global static constructor of Genode application called.\n");
	}

	void dummy() { }
};


extern Testlib_testclass testlib_testobject;

Testapp_testclass testapp_testobject;


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
	env.parent().exit(0);
}
