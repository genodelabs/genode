/*
 * \brief  Blitting function for x86
 * \author Norman Feske
 * \date   2007-10-09
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <blit/blit.h>
#include <mmx.h>


/***************
 ** Utilities **
 ***************/

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
static inline void copy_block_32bit(char *src, int src_w,
                                    char *dst, int dst_w,
                                    int w, int h)
{
	long d0, d1, d2;

	asm volatile ("cld; mov %%ds, %%ax; mov %%ax, %%es" : : : "eax");
	for (; h--; src += src_w, dst += dst_w )
		asm volatile ("rep movsl"
		 : "=S" (d0), "=D" (d1), "=c" (d2)
		 : "S" (src), "D" (dst), "c" (w));
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
	if (w)
		for (int i = h; i--; src += src_w, dst += dst_w)
			copy_32byte_chunks(src, dst, w);
}


/***********************
 ** Library interface **
 ***********************/

extern "C" void blit(void *s, unsigned src_w,
                     void *d, unsigned dst_w,
                     int w, int h)
{
	char *src = (char *)s, *dst = (char *)d;

	if (w <= 0 || h <= 0) return;

	/* we support blitting only at a granularity of 16bit */
	w &= ~1;

	/* copy unaligned column */
	if (w && ((long)dst & 2)) {
		copy_16bit_column(src, src_w, dst, dst_w, h);
		w -= 2; src += 2; dst += 2;
	}

	/* now, we are on a 32bit aligned destination address */

	/* copy 32byte chunks */
	if (w >> 5) {
		copy_block_32byte(src, src_w, dst, dst_w, w >> 5, h);
		src += w & ~31;
		dst += w & ~31;
		w    = w &  31;
	}

	/* copy 32bit chunks */
	if (w >> 2) {
		copy_block_32bit(src, src_w, dst, dst_w, w >> 2, h);
		src += w & ~3;
		dst += w & ~3;
		w    = w &  3;
	}

	/* handle trailing row */
	if (w >> 1) copy_16bit_column(src, src_w, dst, dst_w, h);
}
