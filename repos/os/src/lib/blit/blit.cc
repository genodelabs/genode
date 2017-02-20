/*
 * \brief  Generic blitting function
 * \author Norman Feske
 * \date   2007-10-10
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <blit/blit.h>
#include <blit_helper.h>


extern "C" void blit(void const *s, unsigned src_w,
                     void *d, unsigned dst_w,
                     int w, int h)
{
	char const *src = (char const *)s;
	char       *dst = (char       *)d;

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
