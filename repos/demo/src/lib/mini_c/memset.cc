/*
 * \brief  Mini C memset()
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

using namespace Genode;


extern "C" void *memset(void *d, int c, size_t n)
{
	if (c == 0) {
		bzero(d, n);
		return d;
	}

	for (size_t i = 0; i < n; i++)
		((char *)d)[i] = char(c);

	return d;
}
