/*
 * \brief  Nitpicker string functions
 * \author Norman Feske
 * \date   2006-08-09
 *
 * These custom versions of standard string functions are provided
 * to make Nitpicker independent from the C library. Note that the
 * functions are not 100% compatible with their libC counterparts.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _STRING_H_
#define _STRING_H_

namespace Nitpicker {

	inline void strncpy(char *dst, const char *src, unsigned n)
	{
		unsigned i;
		for (i = 0; (i + 1 < n) && src[i]; i++)
			dst[i] = src[i];

		/* null-terminate string */
		if (n > 0) dst[i] = 0;
	}
}

#endif
