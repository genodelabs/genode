/*
 * \brief  Functor for drawing a refracted icon onto a surface
 * \author Norman Feske
 * \date   2005-10-24
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT_GFX__REFRACTED_ICON_PAINTER_H_
#define _INCLUDE__SCOUT_GFX__REFRACTED_ICON_PAINTER_H_

#include <util/misc_math.h>
#include <util/string.h>
#include <os/texture.h>

namespace Scout { struct Refracted_icon_painter; }

/*
 * NOTE: There is no support for clipping.
 *       Use this code with caution!
 */
struct Scout::Refracted_icon_painter
{
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;


	template <typename DT>
	class Distmap
	{
		private:

			Area      _size;
			DT const *_base;

		public:

			Distmap(DT *base, Area size) : _size(size), _base(base) { }

			Area      size() const { return _size; }
			DT const *base() const { return _base; }
	};


	/**
	 * Copy and distort back-buffer pixels to front buffer
	 */
	template <typename PT, typename DT>
	static void distort(PT const *src, DT const *distmap,
	                    int distmap_w, int distmap_h,
	                    PT const *fg, unsigned char const *alpha,
	                    PT *dst, int dst_w, int width)
	{
		int line_offset = (distmap_w>>1) - width;
		width <<= 1;

		for (int j = 0; j < distmap_h; j += 2, dst += dst_w) {

			PT *d = dst;

			for (int i = 0; i < width; i += 2, src += 2, distmap += 2) {

				/* fetch distorted pixel from back buffer */
				PT v = PT::avr(src[distmap[0]],
				               src[distmap[1] + 1],
				               src[distmap[distmap_w] + distmap_w],
				               src[distmap[distmap_w + 1] + distmap_w + 1]);

				/* mix back-buffer pixel with foreground */
				*d++ = PT::mix(v, *fg++, *alpha++);
			}

			fg      += line_offset;
			alpha   += line_offset;
			src     += line_offset*2 + distmap_w;   /* skip one line in back buffer */
			distmap += line_offset*2 + distmap_w;   /* skip one line in distmap     */
		}
	}


	/**
	 * Copy back-buffer pixels to front buffer
	 */
	template <typename PT>
	static void copy(PT const *src, int src_w, PT *dst, int dst_w, int w, int h)
	{
		for (int j = 0; j < h; j ++, src += src_w, dst += dst_w)
			Genode::memcpy(dst, src, w*sizeof(PT));
	}


	/**
	 * Backup original (background) pixel data into back buffer
	 */
	template <typename PT>
	static void filter_src_to_backbuf(PT const *src, int src_w,
	                                  PT *dst, int dst_w, int dst_h, int width)
	{
		for (int j = 0; j < (dst_h>>1); j++, src += src_w, dst += 2*dst_w) {
			for (int i = 0; i < width; i++) {
				dst[2*i] = src[i];
				dst[2*i + 1] = PT::avr(src[i], src[i + 1]);
				dst[2*i + dst_w] = PT::avr(src[i], src[i + src_w]);
				dst[2*i + dst_w + 1] = PT::avr(dst[2*i + dst_w], dst[2*i + 1]);
			}
		}
	}


	/**
	 * Backup original (background) pixel data into back buffer
	 */
	template <typename PT>
	static void copy_src_to_backbuf(PT *src, int src_w,
	                                PT *dst, int dst_w, int dst_h, int width)
	{
		for (int j = 0; j < (dst_h>>1); j++, src += src_w, dst += 2*dst_w)
			for (int i = 0; i < width; i++)
				dst[2*i] = dst[2*i + 1] = dst[2*i + dst_w] = dst[2*i + dst_w + 1] = src[i];
	}


	/*
	 * The distmap and tmp textures must be dimensioned with twice the height
	 * and width of the foreground.
	 */
	template <typename PT, typename DT>
	static inline void paint(Genode::Surface<PT>       &surface,
	                         Point                      pos,
	                         Distmap<DT>         const &distmap,
	                         Genode::Texture<PT>       &tmp,
	                         Genode::Texture<PT> const &foreground,
	                         bool                       detail,
	                         bool                       filter_backbuf)
	{
		PT *dst = surface.addr() + surface.size().w()*(pos.y()) + pos.x();

		Rect const clipped = Rect::intersect(surface.clip(), Rect(pos, foreground.size()));

		if (detail == false) {
			copy(foreground.pixel(), foreground.size().w(),
			     dst, surface.size().w(), clipped.w(), foreground.size().h());
			return;
		}

		/* backup old canvas pixels */
		if (filter_backbuf)
			filter_src_to_backbuf(dst, surface.size().w(), tmp.pixel(),
			                      tmp.size().w(), tmp.size().h(),
			                      foreground.size().w());
		else
			copy_src_to_backbuf(dst, surface.size().w(),
			                    tmp.pixel(), tmp.size().w(),
			                    tmp.size().h(), foreground.size().w());

		/* draw distorted pixels back to canvas */
		distort<PT, DT>(tmp.pixel(),
		                distmap.base(), distmap.size().w(), distmap.size().h(),
		                foreground.pixel(), foreground.alpha(),
		                dst, surface.size().w(), clipped.w());
	}
};

#endif /* _INCLUDE__SCOUT_GFX__REFRACTED_ICON_PAINTER_H_ */
