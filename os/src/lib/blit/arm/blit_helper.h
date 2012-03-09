/*
 * \brief  Blitting utilities for ARM
 * \author Stefan Kalkowski
 * \date   2012-03-08
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__BLIT__BLIT_HELPER_H_
#define _LIB__BLIT__BLIT_HELPER_H_

#include <blit/blit.h>

/**
 * Copy single 16bit column
 */
static inline void copy_16bit_column(char *src, int src_w,
                                     char *dst, int dst_w, int h)
{
	for (; h-- > 0; src += src_w, dst += dst_w)
		*(short *)dst = *(short *)src;
}


/**
 * Copy pixel block 32bit-wise
 *
 * \param src    source address
 * \param dst    32bit-aligned destination address
 * \param w      number of 32bit words to copy per line
 * \param h      number of lines to copy
 * \param src_w  width of source buffer in bytes
 * \param dst_w  width of destination buffer in bytes
 */
static void copy_block_32bit(char *src, int src_w,
                             char *dst, int dst_w,
                             int w, int h)
{
	src_w -= w*4;
	dst_w -= w*4;
	for (; h--; src += src_w, dst += dst_w) {
		for (int i = w; i--; src += 4, dst += 4)
			*((int *)dst) = *((int *)src);
	}
}


/**
 * Copy block with a size of multiple of 32 bytes
 *
 * \param w  width in 32 byte chunks to copy per line
 * \param h  number of lines of copy
 */
static inline void copy_block_32byte(char *src, int src_w,
                                     char *dst, int dst_w,
                                     int w, int h)
{
	if (((long)src & 3) || ((long)dst & 3))
		copy_block_32bit(src, src_w, dst, dst_w, w*8, h);
	else {
		src_w -= w*32;
		dst_w -= w*32;
		for (; h--; src += src_w, dst += dst_w)
			for (int i = w; i--;)
				asm volatile ("ldmia %0!, {r3 - r10} \n\t"
				              "stmia %1!, {r3 - r10} \n\t"
				              : "+r" (src), "+r" (dst)
				              :: "r3","r4","r5","r6","r7","r8","r9","r10");
	}
}

#endif /* _LIB__BLIT__BLIT_HELPER_H_ */
