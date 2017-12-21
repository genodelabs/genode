/*
 * \brief  Functor for tinting a surface with a color
 * \author Norman Feske
 * \date   2015-11-13
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TINT_PAINTER_H_
#define _TINT_PAINTER_H_

#include <os/surface.h>
#include <polygon_gfx/interpolate_rgba.h>


struct Tint_painter
{
	typedef Genode::Surface_base::Rect Rect;

	/**
	 * Tint box with specified color
	 *
	 * \param rect   position and size of box to tint
	 * \param color  tinting color
	 */
	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface,
	                         Rect                 rect,
	                         Genode::Color        color)
	{
		Rect clipped = Rect::intersect(surface.clip(), rect);

		if (!clipped.valid()) return;

		/*
		 * Generate lookup table (LUT) for mapping brightness values to colors.
		 * The specified color is used as a fixed point within the LUT. The
		 * other values are interpolated from black over the color to white.
		 */
		enum { LUT_SIZE = 256*3 };
		PT            pixel_lut[LUT_SIZE];
		unsigned char alpha_lut[LUT_SIZE];

		unsigned const lut_idx = color.r + color.g + color.b;

		Polygon::interpolate_rgba(Polygon::Color(0, 0, 0), color,
		                          pixel_lut, alpha_lut,
		                          lut_idx + 1, 0, 0);

		Polygon::interpolate_rgba(color, Polygon::Color(255, 255, 255),
		                          pixel_lut + lut_idx, alpha_lut + lut_idx,
		                          LUT_SIZE - lut_idx, 0, 0);


		PT pix(color.r, color.g, color.b);
		PT *dst, *dst_line = surface.addr() + surface.size().w()*clipped.y1() + clipped.x1();

		for (int w, h = clipped.h() ; h--; dst_line += surface.size().w())
			for (dst = dst_line, w = clipped.w(); w--; dst++) {
				PT const pixel = *dst;
				*dst = pixel_lut[pixel.r() + pixel.g() + pixel.b()];
			}

		surface.flush_pixels(clipped);
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__BOX_PAINTER_H_ */
