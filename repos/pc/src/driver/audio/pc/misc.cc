/*
 * \brief  Misc for debugging
 * \author Sebastian Sumpf
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <os/backtrace.h>

extern "C" void lx_backtrace(void)
{
	Genode::backtrace();
}


extern "C" unsigned long long lx_timestamp(void)
{
	return Genode::Trace::timestamp();
}
