/*
 * \brief  Mini C atol()
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

extern "C" long atol(const char *nptr)
{
	long result = 0;
	Genode::ascii_to(nptr, result);
	return result;
}
