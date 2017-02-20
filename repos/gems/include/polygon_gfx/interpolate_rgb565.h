/*
 * \brief   RGB565-optimized interpolation functions for polygon painting
 * \date    2015-06-19
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__INTERPOLATE_RGB565_H_
#define _INCLUDE__POLYGON_GFX__INTERPOLATE_RGB565_H_

/* Genode includes */
#include <util/dither_matrix.h>
#include <os/pixel_rgb565.h>
#include <polygon_gfx/interpolate_rgba.h>

namespace Polygon {

	using Genode::Pixel_rgb565;

	template <>
	inline void interpolate_rgba(Color, Color, Pixel_rgb565 *,
	                             unsigned char *, unsigned, int, int);
}


/**
 * Specialization that employs dithering
 */
template <>
inline void Polygon::interpolate_rgba(Color start, Color end, Pixel_rgb565 *dst,
                                      unsigned char *dst_alpha,
                                      unsigned num_values, int x, int y)
{
	/* sanity check */
	if (num_values <= 0) return;

	/* use 16.16 fixpoint values for the calculation */
	int r_ascent = ((end.r - start.r)<<16) / (int)num_values,
	    g_ascent = ((end.g - start.g)<<16) / (int)num_values,
	    b_ascent = ((end.b - start.b)<<16) / (int)num_values,
	    a_ascent = ((end.a - start.a)<<16) / (int)num_values;

	/* set start values for color components */
	int r = start.r<<16,
	    g = start.g<<16,
	    b = start.b<<16,
	    a = start.a<<16;

	for ( ; num_values--; dst++, dst_alpha++, x++) {

		int const dither_value = Genode::Dither_matrix::value(x, y) << 12;

		/* combine current color value with existing pixel via alpha blending */
		*dst = Pixel_rgb565::mix(*dst,
		                         Pixel_rgb565((r + dither_value) >> 16,
		                                      (g + dither_value) >> 16,
		                                      (b + dither_value) >> 16),
		                         (a + dither_value) >> 16);

		*dst_alpha += ((255 - *dst_alpha)*(a + dither_value)) >> (16 + 8);

		/* increment color-component values by ascent */
		r += r_ascent;
		g += g_ascent;
		b += b_ascent;
		a += a_ascent;
	}
}

#endif /* _INCLUDE__POLYGON_GFX__INTERPOLATE_RGB565_H_ */
