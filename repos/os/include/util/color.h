/*
 * \brief   Color representation
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__COLOR_H_
#define _INCLUDE__UTIL__COLOR_H_

#include <util/string.h>

namespace Genode {
	struct Color;

	inline size_t ascii_to(const char *, Color &);
}


struct Genode::Color
{
	int r, g, b, a;

	bool is_opaque()      const { return a == 255; }
	bool is_transparent() const { return a == 0; }

	Color(int red, int green, int blue, int alpha = 255)
	: r(red), g(green), b(blue), a(alpha) { }

	Color(): r(0), g(0), b(0), a(0) { }

	bool operator == (Color const &other) const {
		return other.r == r && other.g == g && other.b == b; }

	bool operator != (Color const &other) const {
		return !operator == (other); }
};


/**
 * Convert ASCII string to Color
 *
 * The ASCII string must have the format '#rrggbb'
 *
 * \return number of consumed characters, or 0 if the string contains
 *         no valid color
 */
inline Genode::size_t Genode::ascii_to(const char *s, Genode::Color &result)
{
	/* validate string */
	if (strlen(s) < 7 || *s != '#') return 0;

	enum { HEX = true };

	for (unsigned i = 0; i < 6; i++)
		if (!is_digit(s[i + 1], HEX)) return 0;

	int red   = 16*digit(s[1], HEX) + digit(s[2], HEX),
	    green = 16*digit(s[3], HEX) + digit(s[4], HEX),
	    blue  = 16*digit(s[5], HEX) + digit(s[6], HEX);

	result = Color(red, green, blue);
	return 7;
}

#endif /* _INCLUDE__UTIL__COLOR_H_ */
