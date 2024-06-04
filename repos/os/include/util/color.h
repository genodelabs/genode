/*
 * \brief   Color representation
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__COLOR_H_
#define _INCLUDE__UTIL__COLOR_H_

#include <util/string.h>

namespace Genode {
	struct Color;

	inline size_t ascii_to(const char *, Color &);
}


/**
 * Tuple of red, green, blue, and alpha color components
 */
struct Genode::Color
{
	using channel_t = uint8_t;

	channel_t r {}, g {}, b {}, a {};

	/**
	 * Construct opaque color
	 */
	static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b)
	{
		return { r, g, b, 255 };
	}

	/**
	 * Construct color from component values
	 *
	 * This function is useful when colors are computed from integer values.
	 * Whenever a component value lies outside the value range of 'channel_t',
	 * it is clamped to the minimum/maximum value.
	 */
	static Color clamped_rgba(auto r, auto g, auto b, auto a)
	{
		auto clamped = [] (auto const v)
		{
			return (v < 0) ? uint8_t(0) : (v >= 255) ? uint8_t(255) : uint8_t(v);
		};

		return { clamped(r), clamped(g), clamped(b), clamped(a) };
	}

	/**
	 * Construct opaque color from component values
	 */
	static Color clamped_rgb(auto r, auto g, auto b)
	{
		return clamped_rgba(r, g, b, 255);
	}

	/**
	 * Construct opaque black color
	 */
	static constexpr Color black() { return rgb(0, 0, 0); }

	bool opaque()      const { return a == 255; }
	bool transparent() const { return a == 0; }

	bool operator == (Color const &other) const
	{
		return other.r == r && other.g == g && other.b == b && other.a == a;
	}

	bool operator != (Color const &other) const { return !operator == (other); }

	void print(Output &out) const
	{
		using Genode::print;

		print(out, Char('#'));
		print(out, Hex(r, Hex::OMIT_PREFIX, Hex::PAD));
		print(out, Hex(g, Hex::OMIT_PREFIX, Hex::PAD));
		print(out, Hex(b, Hex::OMIT_PREFIX, Hex::PAD));

		if (a != 255)
			print(out, Hex(a, Hex::OMIT_PREFIX, Hex::PAD));
	}
};


/**
 * Convert ASCII string to Color
 *
 * The ASCII string must have the format '#rrggbb' or '#rrggbbaa'.
 *
 * \return number of consumed characters, or 0 if the string contains
 *         no valid color
 */
inline Genode::size_t Genode::ascii_to(const char *s, Color &result)
{
	size_t const len = strlen(s);

	if (len < 7 || *s != '#') return 0;

	bool const HEX = true;

	auto is_digit = [&] (unsigned i) { return Genode::is_digit(s[i], HEX); };
	auto digit    = [&] (unsigned i) { return Genode::digit(s[i], HEX); };

	for (unsigned i = 0; i < 6; i++)
		if (!is_digit(i + 1)) return 0;

	uint8_t const r = uint8_t(16*digit(1) + digit(2)),
	              g = uint8_t(16*digit(3) + digit(4)),
	              b = uint8_t(16*digit(5) + digit(6));

	bool const has_alpha = (len >= 9) && is_digit(7) && is_digit(8);

	uint8_t const a = has_alpha ? uint8_t(16*digit(7) + digit(8)) : 255;

	result = Color { r, g, b, a };

	return has_alpha ? 9 : 7;
}

#endif /* _INCLUDE__UTIL__COLOR_H_ */
