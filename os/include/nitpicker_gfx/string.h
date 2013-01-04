/*
 * \brief  Convert string to color
 * \author Norman Feske
 * \date   2013-01-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__STRING_H_
#define _INCLUDE__NITPICKER_GFX__STRING_H_

/* Genode includes */
#include <util/string.h>

#include "nitpicker_gfx/color.h"

namespace Genode {

	/**
	 * Convert ASCII string to Color
	 *
	 * The ASCII string must have the format '#rrggbb'
	 *
	 * \return number of consumed characters, or 0 if the string contains
	 *         no valid color
	 */
	template <>
	inline size_t ascii_to<Color>(const char *s, Color *result, unsigned)
	{
		/* validate string */
		if (strlen(s) < 7 || *s != '#') return 0;

		enum { HEX = true };

		for (unsigned i = 0; i < 6; i++)
			if (!is_digit(s[i + 1], HEX)) return 0;

		int red   = 16*digit(s[1], HEX) + digit(s[2], HEX),
		    green = 16*digit(s[3], HEX) + digit(s[4], HEX),
		    blue  = 16*digit(s[5], HEX) + digit(s[6], HEX);

		*result = Color(red, green, blue);
		return 7;
	}
}

#endif /* _INCLUDE__NITPICKER_GFX__STRING_H_ */
