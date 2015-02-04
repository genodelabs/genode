/**
 * \brief  Misc functions
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>


extern "C" void *memset(void *s, int c, Genode::size_t n)
{
	return Genode::memset(s, c, n);
}
