/*
 * \brief  Types and utilities used for 2D memory copy
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__TYPES_H_
#define _INCLUDE__BLIT__TYPES_H_

/* Genode includes */
#include <os/texture.h>
#include <os/surface.h>
#include <os/pixel_rgb888.h>

namespace Blit {

	using namespace Genode;

	using Rect  = Surface_base::Rect;
	using Area  = Surface_base::Area;
	using Point = Surface_base::Point;

	enum class Rotate { R0, R90, R180, R270 };

	struct Flip { bool enabled; };

	static bool swap_w_h(Rotate r) { return r == Rotate::R90 || r == Rotate::R270; }

	static Area transformed(Area a, Rotate rotate)
	{
		return swap_w_h(rotate) ? Area { a.h, a.w } : a;
	}

	static Point transformed(Point p, Area area, Rotate rotate, Flip flip)
	{
		int const w = area.w, h = area.h;

		switch (rotate) {
		case Rotate::R0:                                               break;
		case Rotate::R90:  p = { .x = h - p.y - 1, .y = p.x };         break;
		case Rotate::R180: p = { .x = w - p.x - 1, .y = h - p.y - 1 }; break;
		case Rotate::R270: p = { .x = p.y,         .y = w - p.x - 1 }; break;
		}

		if (flip.enabled)
			p = { int(transformed(area, rotate).w) - p.x - 1, p.y };

		return p;
	}

	static Rect transformed(Rect r, Area area, Rotate rotate, Flip flip)
	{
		auto rect_from_points = [&] (Point p1, Point p2)
		{
			return Rect::compound(Point { min(p1.x, p2.x), min(p1.y, p2.y) },
			                      Point { max(p1.x, p2.x), max(p1.y, p2.y) });
		};
		return rect_from_points(transformed(r.p1(), area, rotate, flip),
		                        transformed(r.p2(), area, rotate, flip));
	}

	static Rect snapped_to_8x8_grid(Rect r)
	{
		return Rect::compound(Point { .x =   r.x1()      & ~0x7,
		                              .y =   r.y1()      & ~0x7 },
		                      Point { .x = ((r.x2() + 8) & ~0x7) - 1,
		                              .y = ((r.y2() + 8) & ~0x7) - 1 });
	}

	static inline bool divisable_by_8x8(Area a) { return ((a.w | a.h) & 0x7) == 0; }

	template <typename B2F>
	static inline void _b2f(uint32_t       *dst, unsigned dst_w,
	                        uint32_t const *src, unsigned src_w,
	                        unsigned w, unsigned h, Rotate rotate)
	{
		switch (rotate) {
		case Rotate::R0:   B2F::r0  (dst, dst_w, src,        w, h); break;
		case Rotate::R90:  B2F::r90 (dst, dst_w, src, src_w, w, h); break;
		case Rotate::R180: B2F::r180(dst, dst_w, src,        w, h); break;
		case Rotate::R270: B2F::r270(dst, dst_w, src, src_w, w, h); break;
		}
	}

	template <typename OP>
	static inline void _b2f(Surface<Pixel_rgb888>       &surface,
	                        Texture<Pixel_rgb888> const &texture,
	                        Rect rect, Rotate rotate, Flip flip)
	{
		/* check compatibility of surface with texture */
		if (transformed(surface.size(), rotate) != texture.size()) {
			warning("surface ", surface.size(), " mismatches texture ", texture.size());
			return;
		}

		/* snap src coordinates to multiple of px, restrict to texture size */
		Rect const src_rect = Rect::intersect(snapped_to_8x8_grid(rect),
		                                      Rect { { }, texture.size() });

		/* compute base addresses of affected pixel window */
		Rect const dst_rect = transformed(src_rect, texture.size(), rotate, flip);

		uint32_t const * const src = (uint32_t const *)texture.pixel()
		                           + src_rect.y1()*texture.size().w
		                           + src_rect.x1();

		uint32_t       * const dst = (uint32_t *)surface.addr()
		                           + dst_rect.y1()*surface.size().w
		                           + dst_rect.x1();

		unsigned const src_w = texture.size().w,
		               dst_w = surface.size().w,
		               w     = src_rect.area.w,
		               h     = src_rect.area.h;

		if (w && h) {
			if (flip.enabled)
				_b2f<typename OP::B2f_flip>(dst, dst_w, src, src_w, w, h, rotate);
			else
				_b2f<typename OP::B2f>     (dst, dst_w, src, src_w, w, h, rotate);
		}

		surface.flush_pixels(dst_rect);
	}
}


/****************
 ** Legacy API **
 ****************/

/**
 * Blit memory from source buffer to destination buffer
 *
 * \param src    address of source buffer
 * \param src_w  line length of source buffer in bytes
 * \param dst    address of destination buffer
 * \param dst_w  line length of destination buffer in bytes
 * \param w      number of bytes per line to copy
 * \param h      number of lines to copy
 *
 * This function works at a granularity of 16bit.
 * If the source and destination overlap, the result
 * of the copy operation is not defined.
 */
extern "C" void blit(void const *src, unsigned src_w,
                     void *dst, unsigned dst_w, int w, int h);

#endif /* _INCLUDE__BLIT__TYPES_H_ */
