/*
 * \brief  Test if global static constructors in hybrid applications get called
 * \author Christian Prochaska
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


#include <base/printf.h>

using namespace Genode;


struct Testapp_testclass
{
	Testapp_testclass()
	{
		Genode::printf("Global static constructor of Genode application called\n");
	}
};


Testapp_testclass testapp_testclass;


int main(int argc, char *argv[])
{
	printf("--- lx_hybrid global static constructor test ---\n");

	printf("--- returning from main ---\n");

	return 0;
}
