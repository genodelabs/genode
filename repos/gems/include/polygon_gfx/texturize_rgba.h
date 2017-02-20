/*
 * \brief   Texturizing function for polygon painting
 * \date    2015-06-29
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__TEXTURIZE_RGBA_H_
#define _INCLUDE__POLYGON_GFX__TEXTURIZE_RGBA_H_

/* Genode includes */
#include <util/color.h>
#include <os/pixel_rgba.h>
#include <util/geometry.h>

namespace Polygon {

	template <typename PT>
	static inline void texturize_rgba(Genode::Point<>, Genode::Point<>, PT *,
	                                  unsigned char *, unsigned, PT const *,
	                                  unsigned char const *, unsigned);
}


/**
 * Texturize scanline
 */
template <typename PT>
static inline void Polygon::texturize_rgba(Genode::Point<> start, Genode::Point<> end,
                                           PT *dst, unsigned char *dst_alpha,
                                           unsigned num_values,
                                           PT const *texture_base,
                                           unsigned char const *alpha_base,
                                           unsigned texture_width)
{
	/* sanity check */
	if (num_values <= 0) return;

	/* use 16.16 fixpoint values for the calculation */
	int tx_ascent = ((end.x() - start.x())<<16)/(int)num_values,
	    ty_ascent = ((end.y() - start.y())<<16)/(int)num_values;

	/* set start values for color components */
	int tx = start.x()<<16,
	    ty = start.y()<<16;

	for ( ; num_values--; dst++, dst_alpha++) {

		/* blend pixel from texture with destination point on surface */
		unsigned long src_offset = (ty>>16)*texture_width + (tx>>16);

		int const a = alpha_base[src_offset];

		*dst        = texture_base[src_offset];
		*dst_alpha += ((255 - *dst_alpha)*a) >> 8;

		/* walk through texture */
		tx += tx_ascent;
		ty += ty_ascent;
	}
}

#endif /* _INCLUDE__POLYGON_GFX__TEXTURIZE_RGBA_H_ */
