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

#ifndef _INCLUDE__OS__DITHER_PAINTER_H_
#define _INCLUDE__OS__DITHER_PAINTER_H_

/* Genode includes */
#include <util/dither_matrix.h>
#include <os/surface.h>
#include <os/texture.h>


struct Dither_painter
{
	/*
	 * Surface and texture must have the same size
	 */
	template <typename DST_PT, typename SRC_PT>
	static inline void paint(Genode::Surface<DST_PT>           &surface,
	                         Genode::Texture<SRC_PT>     const &texture,
	                         Genode::Surface_base::Point const &pos)
	{
		Genode::Surface_base::Rect const clipped = surface.clip();

		if (!clipped.valid()) return;

		using Genode::min;
		using Genode::max;

		unsigned const dst_x = max(pos.x(), clipped.x1());
		unsigned const dst_y = max(pos.y(), clipped.y1());

		unsigned const dst_line_len = surface.size().w();
		unsigned const dst_offset = dst_line_len*dst_y + dst_x;

		unsigned const src_line_len = texture.size().w();
		unsigned const src_offset = src_line_len*clipped.y1() + clipped.x1();

		DST_PT              *dst,       *dst_line       = surface.addr()  + dst_offset;
		SRC_PT        const *src_pixel, *src_pixel_line = texture.pixel() + src_offset;
		unsigned char const *src_alpha, *src_alpha_line = texture.alpha() + src_offset;
		bool          const  src_has_alpha = texture.alpha() != nullptr;

		unsigned const x_max = min((unsigned)clipped.x2(), dst_x + texture.size().w() - 1);
		unsigned const y_max = min((unsigned)clipped.y2(), dst_y + texture.size().h() - 1);

		for (unsigned y = dst_y; y <= y_max; y++) {

			src_pixel = src_pixel_line;
			src_alpha = src_alpha_line;
			dst       = dst_line;

			if (src_has_alpha) {
				for (unsigned x = dst_x; x <= x_max; x++) {

					int const v = Genode::Dither_matrix::value(x, y) >> 4;

					SRC_PT        const pixel = *src_pixel++;
					unsigned char const alpha = *src_alpha++;

					int const r = pixel.r() - v;
					int const g = pixel.g() - v;
					int const b = pixel.b() - v;
					int const a = alpha ? (int)alpha - v : 0;

					*dst++ = DST_PT(max(0, r), max(0, g), max(0, b), max(0, a));
				}
			} else {
				for (unsigned x = dst_x; x <= x_max; x++) {

					int const v = Genode::Dither_matrix::value(x, y) >> 4;

					SRC_PT const pixel = *src_pixel++;

					int const r = pixel.r() - v;
					int const g = pixel.g() - v;
					int const b = pixel.b() - v;

					*dst++ = DST_PT(max(0, r), max(0, g), max(0, b));
				}
			}

			src_pixel_line += src_line_len;
			src_alpha_line += src_line_len;
			dst_line       += dst_line_len;
		}
	}
};

#endif /* _INCLUDE__OS__DITHER_PAINTER_H_ */
