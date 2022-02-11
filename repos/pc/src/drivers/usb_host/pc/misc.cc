/*
 * \brief  Misc
 * \author Josef Soentgen
 * \date   2022-01-20
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
