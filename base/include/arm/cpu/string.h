/*
 * \brief  Cpu specifi memcpy
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM__CPU__STRING_H_
#define _INCLUDE__ARM__CPU__STRING_H_

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
		if (size < 32 || (d_align | s_align) || (d_align ^ s_align))
			return size;

		/* copy to 4 byte alignment */
		for (size_t i = 0; i < s_align; i++, *d++ = *s++, size--);

		/* copy 32 byte chunks */
		for (; size >= 32; size -= 32) {
			asm volatile ("ldmia %0!, {r3 - r10} \n\t"
			              "stmia %1!, {r3 - r10} \n\t"
			              : "+r" (s), "+r" (d)
			              :: "r3","r4","r5","r6","r7","r8","r9","r10");
		}
	
		return size;
	}
}

#endif /* _INCLUDE__ARM__CPU__STRING_H_ */
