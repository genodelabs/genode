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

extern "C" void *memset(void *s, int c, Genode::size_t n)
{
	return Genode::memset(s, c, n);
}
