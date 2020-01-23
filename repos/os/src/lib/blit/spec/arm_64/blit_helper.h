/*
 * \brief  Blitting utilities for ARM 64-Bit
 * \author Stefan Kalkowski
 * \date   2020-01-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__BLIT__SPEC__ARM_64__BLIT_HELPER_H_
#define _LIB__BLIT__SPEC__ARM_64__BLIT_HELPER_H_

#include <base/stdint.h>
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
 * Copy pixel block 64bit-wise
 *
 * \param src    source address
 * \param dst    64bit-aligned destination address
 * \param w      number of 32bit words to copy per line
 * \param h      number of lines to copy
 * \param src_w  width of source buffer in bytes
 * \param dst_w  width of destination buffer in bytes
 */
static inline void copy_block_64bit(char const *src, int src_w,
                                    char *dst, int dst_w,
                                    int w, int h)
{
	src_w -= w*8;
	dst_w -= w*8;
	for (; h--; src += src_w, dst += dst_w) {
		for (int i = w; i--; src += 8, dst += 8)
			*((Genode::uint64_t *)dst) = *((Genode::uint64_t const *)src);
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
			copy_block_64bit(src, src_w, dst, dst_w, w*4, 1);
			src += src_w;
			dst += dst_w;
	}
}

#endif /* _LIB__BLIT__SPEC__ARM_64__BLIT_HELPER_H_ */
