/*
 * \brief   Interpolation functions for polygon painting
 * \date    2015-06-19
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__INTERPOLATE_RGBA_H_
#define _INCLUDE__POLYGON_GFX__INTERPOLATE_RGBA_H_

/* Genode includes */
#include <util/color.h>

namespace Polygon {

	using Genode::Color;

	template <typename PT>
	static inline void interpolate_rgba(Color, Color, PT *, unsigned char *,
	                                    unsigned, int, int);
}


/**
 * Interpolate color values
 */
template <typename PT>
static inline void Polygon::interpolate_rgba(Polygon::Color start,
                                             Polygon::Color end,
                                             PT *dst,
                                             unsigned char *dst_alpha,
                                             unsigned num_values,
                                             int, int)
{
	/* sanity check */
	if (num_values == 0) return;

	/* use 16.16 fixpoint values for the calculation */
	int const r_ascent = ((end.r - start.r)<<16) / (int)num_values,
	          g_ascent = ((end.g - start.g)<<16) / (int)num_values,
	          b_ascent = ((end.b - start.b)<<16) / (int)num_values,
	          a_ascent = ((end.a - start.a)<<16) / (int)num_values;

	/* set start values for color components */
	int r = start.r<<16,
	    g = start.g<<16,
	    b = start.b<<16,
	    a = start.a<<16;

	for ( ; num_values--; dst++, dst_alpha++) {

		/* combine current color value with existing pixel via alpha blending */
		*dst        = PT::mix(*dst, PT(r>>16, g>>16, b>>16), a>>16);
		*dst_alpha += ((255 - *dst_alpha)*a) >> (16 + 8);

		/* increment color-component values by ascent */
		r += r_ascent;
		g += g_ascent;
		b += b_ascent;
		a += a_ascent;
	}
}

#endif /* _INCLUDE__POLYGON_GFX__INTERPOLATE_RGBA_H_ */
