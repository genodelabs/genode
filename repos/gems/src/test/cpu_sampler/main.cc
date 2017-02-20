/*
 * \brief  Test for the CPU sampler component
 * \author Christian Prochaska
 * \date   2016-01-18
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

void __attribute((noinline)) func()
{
	for (;;) { }
}

int main(int argc, char *argv[])
{
	Genode::log("Test started. func: ", func);

	func();

	return 0;
}
