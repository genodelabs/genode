/*
 * \brief  Blitting utilities for ARM
 * \author Stefan Kalkowski
 * \date   2012-03-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__BLIT__SPEC__ARM__BLIT_HELPER_H_
#define _LIB__BLIT__SPEC__ARM__BLIT_HELPER_H_

#include <blit/blit.h>

/**
 * Copy single 16bit column
 */
static inline void copy_16bit_column(char const *src, int src_w,
                                     char *dst, int dst_w, int h)
{
	for (; h-- > 0; src += src_w, dst += dst_w)
		*(short *)dst = *(short const *)src;
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
static inline void copy_block_32bit(char const *src, int src_w,
                                    char *dst, int dst_w,
                                    int w, int h)
{
	src_w -= w*4;
	dst_w -= w*4;
	for (; h--; src += src_w, dst += dst_w) {
		for (int i = w; i--; src += 4, dst += 4)
			*((int *)dst) = *((int const *)src);
	}
}


/**
 * Copy block with a size of multiple of 32 bytes
 *
 * \param w  width in 32 byte chunks to copy per line
 * \param h  number of lines of copy
 */
static inline void copy_block_32byte(char const *src, int src_w,
                                     char *dst, int dst_w,
                                     int w, int h)
{
	for (; h > 0; h--) {
		/*
		 * Depending on 'src_w' and 'dst_w', some lines may be properly aligned,
		 * while others may be not, so we need to check each line.
		 */
		if (((long)src & 3) || ((long)dst & 3)) {
			copy_block_32bit(src, src_w, dst, dst_w, w*8, 1);
			src += src_w;
			dst += dst_w;
		} else {
			for (int i = w; i > 0; i--)
				asm volatile ("ldmia %0!, {r3 - r10} \n\t"
				              "stmia %1!, {r3 - r10} \n\t"
				              : "+r" (src), "+r" (dst)
				              :: "r3","r4","r5","r6","r7","r8","r9","r10");
			/*
			 * 'src' and 'dst' got auto-incremented by the copy code, so only
			 * the remainder needs to get added
			 */
			src += (src_w - w*32);
			dst += (dst_w - w*32);
		}
	}
}

#endif /* _LIB__BLIT__SPEC__ARM__BLIT_HELPER_H_ */
