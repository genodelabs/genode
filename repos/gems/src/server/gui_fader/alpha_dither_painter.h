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
	using Pixel_alpha8 = Genode::Pixel_alpha8;
	using Rect         = Genode::Surface_base::Rect;
	using Point        = Genode::Surface_base::Point;
	using uint8_t      = Genode::uint8_t;

	struct Alpha_tile_16x16
	{
		uint8_t v[256];

		static Alpha_tile_16x16 from_fade_value(int fade)
		{
			using namespace Genode;

			fade *= 256;  /* scale fade value to range of alpha values */

			Alpha_tile_16x16 t;
			for (unsigned y = 0; y < 16; y++) {
				for (unsigned x = 0; x < 16; x++) {
					int const v = Dither_matrix::value(x, y) << 13;
					t.v[y*16 + x] = (uint8_t)min(255, max(0, (fade - v) >> 16));
				}
			}
			return t;
		}
	};

	/*
	 * \param fade  fade value in 16.16 fixpoint format
	 */
	static inline void paint(Genode::Surface<Pixel_alpha8> &surface,
	                         Rect rect, int fade)
	{
		Rect clipped = Rect::intersect(surface.clip(), rect);

		if (!clipped.valid()) return;

		Pixel_alpha8 *dst, *dst_line = surface.addr() + surface.size().w*clipped.y1() + clipped.x1();

		int y = clipped.y1();

		Alpha_tile_16x16 const tile = Alpha_tile_16x16::from_fade_value(fade);

		for (int w, h = clipped.h() ; h--; dst_line += surface.size().w, y++) {

			int x = clipped.x1();
			unsigned const y_offset = y << 4;

			for (dst = dst_line, w = clipped.w(); w--; dst++, x++)
				dst->pixel = tile.v[(y_offset + x) & 0xff];
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

		unsigned const src_line_w = texture.size().w,
		               dst_line_w = surface.size().w;

		unsigned const src_start = src_line_w*clipped.y1() + clipped.x1(),
		               dst_start = dst_line_w*clipped.y1() + clipped.x1();

		Pixel_alpha8 *src, *src_line = (Pixel_alpha8 *)texture.alpha() + src_start;
		Pixel_alpha8 *dst, *dst_line = surface.addr()                  + dst_start;

		int y = clipped.y1();

		for (int w, h = clipped.h() ; h--; dst_line += dst_line_w, src_line += src_line_w) {

			int x = clipped.x1();

			for (dst = dst_line, src = src_line, w = clipped.w(); w--; dst++, src++, x++) {

				using namespace Genode;

				/*
				 * Multiply texture alpha value with fade value, dither the
				 * result.
				 */

				int const a = (((int)src->pixel)*fade);
				int const v = Dither_matrix::value(x, y) << 13;

				dst->pixel = (uint8_t)min(255, max(0, (a - v) >> 16));
			}

			y++;
		}
	}
};

#endif /* _ALPHA_DITHER_PAINTER_H_ */
