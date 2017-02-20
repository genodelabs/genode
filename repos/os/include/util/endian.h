/*
 * \brief  Convert between host endianess and big endian.
 * \author Stebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2010-08-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _UTIL__ENDIAN_H_
#define _UTIL__ENDIAN_H_

#include <base/stdint.h>

template <typename T>
inline T host_to_big_endian(T x)
{
	Genode::uint8_t v[sizeof(T)];
	for (unsigned i=0; i<sizeof(T); i++) {
		unsigned mask = (sizeof(T) - i - 1) * 8;
		v[i] = (x & (0xff << mask)) >> mask;
	}
	return *(T *)v;
}

#endif /* _UTIL__ENDIAN_H_ */
