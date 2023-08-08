/*
 * \brief  Convert between host endianess (little endian) and big endian.
 * \author Stebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2010-08-04
 */

/*
 * Copyright (C) 2010-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _UTIL__ENDIAN_H_
#define _UTIL__ENDIAN_H_

#include <base/stdint.h>

template <typename T>
inline T swap_bytes(T x)
{
	Genode::uint8_t v[sizeof(T)];

	for (unsigned i = 0; i < sizeof(T); i++) {

		unsigned const shift = ((unsigned)sizeof(T) - i - 1) * 8;

		v[i] = (Genode::uint8_t)((x & ((T)0xff << shift)) >> shift);
	}
	return *(T *)v;
}

template <typename T>
inline T host_to_big_endian(T x)
{
	return swap_bytes(x);
}

template <typename T>
inline T big_endian_to_host(T x)
{
	return swap_bytes(x);
}

#endif /* _UTIL__ENDIAN_H_ */
