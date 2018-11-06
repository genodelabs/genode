/*
 * \brief  Functor for converting pixel formats by applying dithering
 * \author Norman Feske
 * \date   2014-09-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DITHER_PAINTER_H_
#define _DITHER_PAINTER_H_

#include <util/dither_matrix.h>
#include <os/surface.h>


struct Dither_painter
{
	/*
	 * Surface and texture must have the same size
	 */
	template <typename DST_PT, typename SRC_PT>
	static inline void paint(Genode::Surface<DST_PT>       &surface,
	                         Genode::Texture<SRC_PT> const &texture)
	{
		if (surface.size() != texture.size()) return;
		if (!texture.pixel() || !texture.alpha()) return;

		Genode::Surface_base::Rect const clipped = surface.clip();

		if (!clipped.valid()) return;

		unsigned const offset = surface.size().w()*clipped.y1() + clipped.x1();

		DST_PT              *dst,       *dst_line       = surface.addr()  + offset;
		SRC_PT        const *src_pixel, *src_pixel_line = texture.pixel() + offset;
		unsigned char const *src_alpha, *src_alpha_line = texture.alpha() + offset;

		unsigned const line_len = surface.size().w();

		for (int y = clipped.y1(), h = clipped.h() ; h--; y++) {

			src_pixel = src_pixel_line;
			src_alpha = src_alpha_line;
			dst       = dst_line;

			for (int x = clipped.x1(), w = clipped.w(); w--; x++) {

				int const v = Genode::Dither_matrix::value(x, y) >> 4;

				SRC_PT        const pixel = *src_pixel++;
				unsigned char const alpha = *src_alpha++;

				int const r = pixel.r() - v;
				int const g = pixel.g() - v;
				int const b = pixel.b() - v;
				int const a = alpha ? (int)alpha - v : 0;

				using Genode::min;
				using Genode::max;

				*dst++ = DST_PT(max(0, r), max(0, g), max(0, b), max(0, a));
			}

			src_pixel_line += line_len;
			src_alpha_line += line_len;
			dst_line       += line_len;
		}
	}
};

#endif /* _DITHER_PAINTER_H_ */
