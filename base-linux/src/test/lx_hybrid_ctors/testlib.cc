/*
 * \brief  Test if global static constructors in host shared libs get called
 * \author Christian Prochaska
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <stdio.h>

struct Testlib_testclass
{
	Testlib_testclass()
	{
		printf("[init -> test-lx_hybrid_ctors] Global static constructor of host library called.\n");
	}
};

Testlib_testclass testlib_testclass;
