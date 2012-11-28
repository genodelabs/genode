/*
 * \brief  Cpu specifi memcpy
 * \author Martin Stein
 * \date   2012-11-27
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_MB__INCLUDE__CPU__STRING_H_
#define _BASE_MB__INCLUDE__CPU__STRING_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Copy memory block
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to copy
	 *
	 * \return      Number of bytes not copied
	 */
	inline size_t memcpy_cpu(void *dst, const void *src, size_t size)
	{
		unsigned char *d = (unsigned char *)dst, *s = (unsigned char *)src;

		/* check 4 byte; alignment */
		size_t d_align = (size_t)d & 0x3;
		size_t s_align = (size_t)s & 0x3;

		/* at least 32 bytes, 4 byte aligned, same alignment */
		if (size < 32 || (d_align ^ s_align))
			return size;

		/* copy to 4 byte alignment */
		for (size_t i = 0; i < s_align; i++, *d++ = *s++, size--);

		/* copy words */
		uint32_t * dw = (uint32_t *)d;
		uint32_t * sw = (uint32_t *)s;
		for (; size >= 4; size -= 4, dw++, sw++) *dw = *sw;

		return size;
	}
}

#endif /* _BASE_MB__INCLUDE__CPU__STRING_H_ */
