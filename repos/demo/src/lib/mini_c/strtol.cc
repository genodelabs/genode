/*
 * \brief  Mini C strtol()
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/string.h>

using namespace Genode;

extern "C" long int strtol(const char *nptr, char **endptr, int base)
{
	long num_chars, result = 0;

	if (base == 0)
		num_chars = ascii_to(nptr, &result);
	else
		num_chars = ascii_to(nptr, &result, base);

	if (endptr)
		*endptr = (char *)(nptr + num_chars);

	return result;
}
