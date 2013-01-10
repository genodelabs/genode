/*
 * \brief  Functions required for using the arm-none-linux-gnueabi tool chain
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/stdint.h>

using namespace Genode;


extern "C" int raise(int sig)
{
	PWRN("raise - not yet implemented");
	return 0;
}
