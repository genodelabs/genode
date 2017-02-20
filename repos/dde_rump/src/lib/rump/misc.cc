/**
 * \brief  Misc functions
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>


/*
 * On some platforms (namely ARM) we end-up pulling in string.h prototypes
 */
extern "C" void *memcpy(void *d, void *s, Genode::size_t n)
{
	return Genode::memcpy(d, s, n);
}


extern "C" void *memset(void *s, int c, Genode::size_t n)
{
	return Genode::memset(s, c, n);
}
