/*
 * \brief  Wifi front end utilities
 * \author Josef Soentgen
 * \date   2018-07-23
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__UTIL_H_
#define _WIFI__UTIL_H_

/* Genode includes */
#include <util/string.h>

typedef unsigned long  size_t;
typedef long long      ssize_t;
typedef unsigned char  uint8_t;


namespace Util {

	using size_t = Genode::size_t;
	using uint8_t = Genode::uint8_t;

	size_t next_char(char const *s, size_t start, char const c)
	{
		size_t v = start;
		while (s[v]) {
			if (s[v] == c) { break; }
			v++;
		}
		return v - start;
	}

	bool string_contains(char const *str, char const *pattern)
	{
		char const *p = pattern;
		while (*str && *p) {
			p = *str == *p ? p + 1 : pattern;
			str++;
		}
		return !*p;
	}

	void byte2hex(char *dest, uint8_t b)
	{
		int i = 1;
		if (b < 16) { dest[i--] = '0'; }

		for (; b > 0; b /= 16) {
			uint8_t const v = b % 16;
			uint8_t const c = (v > 9) ? v + 'a' - 10 : v + '0';
			dest[i--] = (char)c;
		}
	}

	/**********************************
	 ** Front end specific utilities **
	 **********************************/

	inline unsigned approximate_quality(char const *str)
	{
		long level = 0;
		Genode::ascii_to(str, level);

		/*
		 * We provide an quality value by transforming the actual
		 * signal level [-50,-100] (dBm) to [100,0] (%).
		 */
		if      (level <= -100) { return   0; }
		else if (level >=  -50) { return 100; }

		return 2 * (unsigned)(level + 100);
	}

	inline Genode::uint64_t check_time(Genode::uint64_t value, Genode::uint64_t min, Genode::uint64_t max)
	{
		if      (value < min) { return min; }
		else if (value > max) { return max; }
		return value;
	}

} /* namespace Util */

#endif /* _WIFI__UTIL_H_ */
