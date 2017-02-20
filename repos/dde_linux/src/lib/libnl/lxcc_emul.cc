/**
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-07-28
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>


/**************
 ** stdlib.h **
 **************/

static char getenv_NLDBG[]          = "1";
static char getenv_HZ[]             = "100";
static char getenv_TICKS_PER_USEC[] = "10000";


extern "C" char *getenv(const char *name)
{
	if (Genode::strcmp(name, "NLDBG") == 0)          return getenv_NLDBG;
	if (Genode::strcmp(name, "HZ") == 0)             return getenv_HZ;
	if (Genode::strcmp(name, "TICKS_PER_USEC") == 0) return getenv_TICKS_PER_USEC;

	return nullptr;
}
