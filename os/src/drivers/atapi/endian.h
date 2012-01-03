/*
 * \brief  Convert host endianess to big endian
 * \author Sebastian Sumpf
 * \date   2010-08-04
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _ENDIAN_H_
#define _ENDIAN_H_

template <typename T>
inline T bswap32(T x)
{
	char v[4] = {
		(x & 0xff000000) >> 24,
		(x & 0x00ff0000) >> 16,
		(x & 0x0000ff00) >>  8,
		(x & 0x000000ff) >>  0,
	};
	return *(T *)v;
}

#endif /* _ENDIAN_H_ */

