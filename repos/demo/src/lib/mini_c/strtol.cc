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
#include <base/log.h>


extern "C" long int strtol(const char *nptr, char **endptr, int base)
{
	using namespace Genode;

	long result = 0;

	if (base != 0 && base != 10) {
		error("strtol: base of ", base, " not supported");
		return 0;
	}

	long const num_chars = ascii_to(nptr, result);

	if (endptr)
		*endptr = (char *)(nptr + num_chars);

	return result;
}
