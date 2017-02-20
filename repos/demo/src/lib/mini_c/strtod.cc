/*
 * \brief  Mini C strtod()
 * \author Christian Helmuth
 * \date   2008-07-24
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>

extern "C" double strtod(const char *nptr, char **endptr)
{
	double value = 0;

	int num_chars = Genode::ascii_to(nptr, value);

	if (endptr)
		*endptr = (char *)(nptr + num_chars);

	return value;
}

