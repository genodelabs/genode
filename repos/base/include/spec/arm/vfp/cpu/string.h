/*
 * \brief  ARM-specific memcpy using VFP
 * \author Sebastian Sumpf
 * \date   2013-06-19
 *
 * Should work for VFPv2, VFPv3, and Advanced SIMD.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__VFP__CPU__STRING_H_
#define _INCLUDE__SPEC__ARM__VFP__CPU__STRING_H_

namespace Genode {

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

		/* only same alignments work for the following loops */
		if (d_align != s_align)
			return size;

		/* copy to 4 byte alignment */
		for (; (size > 0) && (s_align > 0) && (s_align < 4);
		     s_align++, *d++ = *s++, size--);

		/* copy 64 byte chunks using FPU */
		for (; size >= 64; size -= 64)
			asm volatile ("pld [%0, #0xc0]  \n\t"
			              "vldm %0!,{d0-d7} \n\t"
			              "vstm %1!,{d0-d7} \n\t"
			              : "+r"(s), "+r" (d)
                    :: "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7");

		/* copy left over 32 byte chunk */
		for (; size >= 32; size -= 32)
			asm volatile ("ldmia %0!, {r3 - r10} \n\t"
			              "stmia %1!, {r3 - r10} \n\t"
			              : "+r" (s), "+r" (d)
			              :: "r3","r4","r5","r6","r7","r8","r9","r10");

		for(; size >= 4; size -= 4)
			asm volatile ("ldr r3, [%0], #4 \n\t"
			              "str r3, [%1], #4 \n\t"
			              : "+r" (s), "+r" (d)
			              :: "r3");
		return size;
	}
}

#endif /* _INCLUDE__SPEC__ARM__VFP__CPU__STRING_H_ */
