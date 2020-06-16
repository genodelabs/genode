/*
 * \brief  Functor for drawing dithered alpha values
 * \author Norman Feske
 * \date   2014-09-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ALPHA_DITHER_PAINTER_H_
#define _ALPHA_DITHER_PAINTER_H_

#include <util/dither_matrix.h>
#include <os/surface.h>
#include <os/pixel_alpha8.h>


struct Alpha_dither_painter
{
	typedef Genode::Pixel_alpha8 Pixel_alpha8;

	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Surface_base::Point Point;

	/*
	 * \param fade  fade value in 16.16 fixpoint format
	 */
	static inline void paint(Genode::Surface<Pixel_alpha8> &surface,
	                         Rect rect, int fade)
	{
		Rect clipped = Rect::intersect(surface.clip(), rect);

		if (!clipped.valid()) return;

		Pixel_alpha8 *dst, *dst_line = surface.addr() + surface.size().w()*clipped.y1() + clipped.x1();

		int y = clipped.y1();

		/* scale fade value to range of alpha values */
		fade *= 256;

		for (int w, h = clipped.h() ; h--; dst_line += surface.size().w()) {

			int x = clipped.x1();

			for (dst = dst_line, w = clipped.w(); w--; dst++, x++) {

				int const v = Genode::Dither_matrix::value(x, y) << 13;

				dst->pixel = Genode::min(255, Genode::max(0, (fade - v) >> 16));
			}

			y++;
		}
	}

	template <typename TPT>
	static inline void paint(Genode::Surface<Pixel_alpha8> &surface,
	                         Rect rect, int fade,
	                         Genode::Texture<TPT> const &texture)
	{
		/* clip against surface size */
		Rect clipped = Rect::intersect(surface.clip(), rect);

		/* clip against texture size */
		clipped = Rect::intersect(Rect(Point(0, 0), texture.size()), clipped);

		if (!clipped.valid()) return;

		unsigned const src_line_w = texture.size().w(),
		               dst_line_w = surface.size().w();

		unsigned const src_start = src_line_w*clipped.y1() + clipped.x1(),
		               dst_start = dst_line_w*clipped.y1() + clipped.x1();

		Pixel_alpha8 *src, *src_line = (Pixel_alpha8 *)texture.alpha() + src_start;
		Pixel_alpha8 *dst, *dst_line = surface.addr()                  + dst_start;

		int y = clipped.y1();

		for (int w, h = clipped.h() ; h--; dst_line += dst_line_w, src_line += src_line_w) {

			int x = clipped.x1();

			for (dst = dst_line, src = src_line, w = clipped.w(); w--; dst++, src++, x++) {

				/*
				 * Multiply texture alpha value with fade value, dither the
				 * result.
				 */

				int const a = (((int)src->pixel)*fade);
				int const v = Genode::Dither_matrix::value(x, y) << 13;

				dst->pixel = Genode::min(255, Genode::max(0, (a - v) >> 16));
			}

			y++;
		}
	}
};

#endif /* _ALPHA_DITHER_PAINTER_H_ */
