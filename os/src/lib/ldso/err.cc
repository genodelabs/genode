/*
 * \brief  Error message handling
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <err.h>
#include <base/env.h>
#include <base/printf.h>


extern "C" void errx(int eval, const char *fmt, ...)
{
	using namespace Genode;
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
	env()->parent()->exit(eval);
	while(1) ;
}
