/*
 * \brief  Functor for drawing an icon onto a surface
 * \author Norman Feske
 * \date   2005-10-24
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT_GFX__ICON_PAINTER_H_
#define _INCLUDE__SCOUT_GFX__ICON_PAINTER_H_

#include <os/texture.h>

class Icon_painter
{
	private:

		/*
		 * An Icon has the following layout:
		 *
		 *  P1---+--------+----+
		 *  | cs |   hs   | cs |   top row
		 *  +----P2-------+----+
		 *  |    |        |    |
		 *  | vs |        | vs |   mid row
		 *  |    |        |    |
		 *  +----+--------P3---+
		 *  | cs |   hs   | cs |   low row
		 *  +------------------P4
		 *
		 * cs ... corner slice
		 * hs ... horizontal slice
		 * vs ... vertical slice
		 */

		/**
		 * Draw corner slice
		 */
		template <typename SPT, typename TPT>
		static void _draw_cslice(TPT const *src, unsigned char const *src_a,
		                         unsigned src_pitch, int alpha, SPT *dst,
		                         unsigned dst_pitch, int w, int h)
		{
			for (int j = 0; j < h; j++) {

				TPT           const *s  = src;
				unsigned char const *sa = src_a;
				SPT                 *d  = dst;

				for (int i = 0; i < w; i++, s++, sa++, d++)
					SPT::transfer(*s, *sa, alpha, *d);

				src += src_pitch, src_a += src_pitch, dst += dst_pitch;
			}
		}


		/**
		 * Draw horizontal slice
		 */
		template <typename SPT, typename TPT>
		static void _draw_hslice(TPT const *src, unsigned char const *src_a,
		                         int src_pitch, int alpha, SPT *dst,
		                         int dst_pitch, int w, int h)
		{
			for (int j = 0; j < h; j++) {

				TPT  s = *src;
				int sa = *src_a;
				SPT *d =  dst;

				for (int i = 0; i < w; i++, d++)
					SPT::transfer(s, sa, alpha, *d);

				src += src_pitch, src_a += src_pitch, dst += dst_pitch;
			}
		}


		/**
		 * Draw vertical slice
		 */
		template <typename SPT, typename TPT>
		static void _draw_vslice(TPT const *src, unsigned char const *src_a,
		                         int /* src_pitch */, int alpha, SPT *dst,
		                         int    dst_pitch,    int w, int h)
		{
			for (int i = 0; i < w; i++) {

				TPT  s = *src;
				int sa = *src_a;
				SPT *d =  dst;

				for (int j = 0; j < h; j++, d += dst_pitch)
					SPT::transfer(s, sa, alpha, *d);

				src += 1, src_a += 1, dst += 1;
			}
		}


		/**
		 * Draw center slice
		 */
		template <typename SPT, typename TPT>
		static void _draw_center(TPT const *src, unsigned char const *src_a,
		                         int /* src_pitch */, int alpha, SPT *dst,
		                         int    dst_pitch,    int w, int h)
		{
			TPT  s = *src;
			int sa = *src_a;

			for (int j = 0; j < h; j++, dst += dst_pitch) {

				SPT *d = dst;

				for (int i = 0; i < w; i++, d++)
					SPT::transfer(s, sa, alpha, *d);
			}
		}


		/**
		 * Clip rectangle against clipping region
		 *
		 * The out parameters are the resulting x/y offsets and the
		 * visible width and height.
		 *
		 * \return  1 if rectangle intersects with clipping region,
		 *          0 otherwise
		 */
		static inline int _clip(int px1, int py1, int px2, int py2,
		                        int cx1, int cy1, int cx2, int cy2,
		                        int *out_x, int *out_y, int *out_w, int *out_h)
		{
			/* determine intersection of rectangle and clipping region */
			int x1 = Genode::max(px1, cx1);
			int y1 = Genode::max(py1, cy1);
			int x2 = Genode::min(px2, cx2);
			int y2 = Genode::min(py2, cy2);

			*out_w = x2 - x1 + 1;
			*out_h = y2 - y1 + 1;
			*out_x = x1 - px1;
			*out_y = y1 - py1;

			return (*out_w > 0) && (*out_h > 0);
		}

	public:

		typedef Genode::Surface_base::Area Area;
		typedef Genode::Surface_base::Rect Rect;


		template <typename SPT, typename TPT>
		static inline void paint(Genode::Surface<SPT> &surface, Rect rect,
		                         Genode::Texture<TPT> const &icon, unsigned alpha)
		{
			SPT *addr = surface.addr();

			if (!addr || (alpha == 0)) return;

			int const cx1 = surface.clip().x1();
			int const cy1 = surface.clip().y1();
			int const cx2 = surface.clip().x2();
			int const cy2 = surface.clip().y2();

			unsigned const icon_w = icon.size().w(),
			               icon_h = icon.size().h();

			/* determine point positions */
			int const x1 = rect.x1();
			int const y1 = rect.y1();
			int const x4 = x1 + rect.w() - 1;
			int const y4 = y1 + rect.h() - 1;
			int const x2 = x1 + icon_w/2;
			int const y2 = y1 + icon_h/2;
			int const x3 = Genode::max(x4 - (int)icon_w/2 + 1, x2);
			int const y3 = Genode::max(y4 - (int)icon_h/2 + 1, y2);

			int const tx1 = 0;
			int const ty1 = 0;
			int const tx4 = icon_w - 1;
			int const ty4 = icon_h - 1;
			int const tx2 = icon_w/2;
			int const ty2 = icon_h/2;
			int const tx3 = Genode::max(tx4 - (int)icon_w/2 + 1, tx2);
			int const ty3 = Genode::max(ty4 - (int)icon_h/2 + 1, ty2);

			TPT           const *src   = icon.pixel() + icon_w*ty1;
			unsigned char const *src_a = icon.alpha() + icon_w*ty1;
			int                  dx, dy, w, h;

			/*
			 * top row
			 */

			if (_clip(x1, y1, x2 - 1, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_cslice(src + tx1 + dy*icon_w + dx, src_a + tx1 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y1 + dy)*surface.size().w() + x1 + dx, surface.size().w(), w, h);

			if (_clip(x2, y1, x3 - 1, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_hslice(src + tx2 + dy*icon_w + dx, src_a + tx2 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y1 + dy)*surface.size().w() + x2 + dx, surface.size().w(), w, h);

			if (_clip(x3, y1, x4, y2 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_cslice(src + tx3 + dy*icon_w + dx, src_a + tx3 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y1 + dy)*surface.size().w() + x3 + dx, surface.size().w(), w, h);

			/*
			 * mid row
			 */

			src   = icon.pixel() + icon_w*ty2;
			src_a = icon.alpha() + icon_w*ty2;

			if (_clip(x1, y2, x2 - 1, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_vslice(src + tx1 + dx, src_a + tx1 + dx, icon_w, alpha,
				            addr + (y2 + dy)*surface.size().w() + x1 + dx, surface.size().w(), w, h);

			if (_clip(x2, y2, x3 - 1, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_center(src + tx2, src_a + tx2, icon_w, alpha,
				            addr + (y2 + dy)*surface.size().w() + x2 + dx, surface.size().w(), w, h);

			if (_clip(x3, y2, x4, y3 - 1, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_vslice(src + tx3 + dx, src_a + tx3 + dx, icon_w, alpha,
				            addr + (y2 + dy)*surface.size().w() + x3 + dx, surface.size().w(), w, h);

			/*
			 * low row
			 */

			src   = icon.pixel() + icon_w*ty3;
			src_a = icon.alpha() + icon_w*ty3;

			if (_clip(x1, y3, x2 - 1, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_cslice(src + tx1 + dy*icon_w + dx, src_a + tx1 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y3 + dy)*surface.size().w() + x1 + dx, surface.size().w(), w, h);

			if (_clip(x2, y3, x3 - 1, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_hslice(src + tx2 + dy*icon_w + dx, src_a + tx2 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y3 + dy)*surface.size().w() + x2 + dx, surface.size().w(), w, h);

			if (_clip(x3, y3, x4, y4, cx1, cy1, cx2, cy2, &dx, &dy, &w, &h))
				_draw_cslice(src + tx3 + dy*icon_w + dx, src_a + tx3 + dy*icon_w + dx, icon_w, alpha,
				            addr + (y3 + dy)*surface.size().w() + x3 + dx, surface.size().w(), w, h);
		}
};

#endif /* _INCLUDE__SCOUT_GFX__ICON_PAINTER_H_ */
