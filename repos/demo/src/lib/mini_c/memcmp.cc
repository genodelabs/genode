/*
 * \brief  Mini C memcmp()
 * \author Christian Prochaska
 * \date   2009-08-11
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>

extern "C" int memcmp(const void *s1, const void *s2, Genode::size_t n)
{
	return Genode::memcmp(s1, s2, n);
}
