/*
 * \brief  Test if global static constructors in host shared libs get called
 * \author Christian Prochaska
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>

#include "testlib.h"


Testlib_testclass::Testlib_testclass()
{
	printf("[init -> test-lx_hybrid_ctors] Global static constructor of host library called.\n");
}


void Testlib_testclass::dummy() { }


Testlib_testclass testlib_testobject;
