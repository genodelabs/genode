/*
 * \brief  Canvas storing each pixel in one storage unit in a linear buffer
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__CHUNKY_CANVAS_H_
#define _INCLUDE__NITPICKER_GFX__CHUNKY_CANVAS_H_

#include <blit/blit.h>
#include "canvas.h"


template <typename PT>
class Chunky_texture : public Texture
{
	private:

		PT            *_pixels;
		unsigned char *_alpha;

	public:

		Chunky_texture(PT *pixels, unsigned char *alpha,  Area size) :
			Texture(size), _pixels(pixels), _alpha(alpha) { }

		PT *pixels() { return _pixels; }

		unsigned char *alpha() { return _alpha; }
};


/*
 * \param PT  pixel type
 */
template <typename PT>
class Chunky_canvas : public Canvas
{
	protected:

		PT *_addr;   /* base address of pixel buffer   */

	public:

		/**
		 * Constructor
		 */
		Chunky_canvas(PT *addr, Area size): Canvas(size), _addr(addr) { }


		/****************************************
		 ** Implementation of Canvas interface **
		 ****************************************/

		void draw_box(Rect rect, Color color)
		{
			Rect clipped = Rect::intersect(_clip, rect);

			if (!clipped.valid()) return;

			PT pix(color.r, color.g, color.b);
			PT *dst, *dst_line = _addr + _size.w()*clipped.y1() + clipped.x1();

			for (int w, h = clipped.h() ; h--; dst_line += _size.w())
				for (dst = dst_line, w = clipped.w(); w--; dst++)
					*dst = pix;

			_flush_pixels(clipped);
		}

		void draw_string(Point p, Font *font, Color color, const char *sstr)
		{
			const unsigned char *str = (const unsigned char *)sstr;
			int x = p.x(), y = p.y();

			if (!str || !font) return;

			unsigned char const *src = font->img;
			int d, h = font->img_h;

			/* check top clipping */
			if ((d = _clip.y1() - y) > 0) {
				src += d*font->img_w;
				y   += d;
				h   -= d;
			}

			/* check bottom clipping */
			if ((d = y + h -1 - _clip.y2()) > 0)
				h -= d;

			if (h < 1) return;

			/* skip hidden glyphs */
			for ( ; *str && (x + font->wtab[*str] < _clip.x1()); )
				x += font->wtab[*str++];

			int x_start = x;

			PT *dst = _addr + y*_size.w();
			PT pix(color.r, color.g, color.b);

			/* draw glyphs */
			for ( ; *str && (x <= _clip.x2()); str++) {

				int                  w     = font->wtab[*str];
				int                  start = max(0, _clip.x1() - x);
				int                  end   = min(w - 1, _clip.x2() - x);
				PT                  *d     = dst + x;
				unsigned char const *s     = src + font->otab[*str];

				for (int j = 0; j < h; j++, s += font->img_w, d += _size.w())
					for (int i = start; i <= end; i++)
						if (s[i]) d[i] = pix;

				x += w;
			}

			_flush_pixels(Rect(Point(x_start, y), Area(x - x_start + 1, h)));
		}

		void draw_texture(Texture *texture, Color mix_color, Point position,
		                  Mode mode, bool allow_alpha)
		{
			Rect clipped = Rect::intersect(Rect(position, *texture), _clip);

			if (!clipped.valid()) return;

			int src_w = texture->w();
			int dst_w = _size.w();

			Chunky_texture<PT> *tex = static_cast<Chunky_texture<PT> *>(texture);

			/* calculate offset of first texture pixel to copy */
			unsigned long tex_start_offset = (clipped.y1() - position.y())*src_w
			                               +  clipped.x1() - position.x();

			/* start address of source pixels */
			PT *src = tex->pixels() + tex_start_offset;

			/* start address of source alpha values */
			unsigned char *alpha = tex->alpha() + tex_start_offset;

			/* start address of destination pixels */
			PT *dst = _addr + clipped.y1()*dst_w + clipped.x1();

			PT mix_pixel(mix_color.r, mix_color.g, mix_color.b);

			int i, j;
			PT *s, *d;
			unsigned char *a;

			switch (mode) {

			case SOLID:

				/*
				 * If the texture has no alpha channel, we can use
				 * a plain pixel blit.
				 */
				if (tex->alpha() == 0 || !allow_alpha) {
					blit(src, src_w*sizeof(PT),
					     dst, dst_w*sizeof(PT),
					     clipped.w()*sizeof(PT), clipped.h());
					break;
				}

				/*
				 * Copy texture with alpha blending
				 */
				for (j = clipped.h(); j--; src += src_w, alpha += src_w, dst += dst_w)
					for (i = clipped.w(), s = src, a = alpha, d = dst; i--; s++, d++, a++)
						if (*a)
							*d = PT::mix(*d, *s, *a);
				break;

			case MIXED:

				for (j = clipped.h(); j--; src += src_w, dst += dst_w)
					for (i = clipped.w(), s = src, d = dst; i--; s++, d++)
						*d = PT::avr(mix_pixel, *s);
				break;

			case MASKED:

				for (j = clipped.h(); j--; src += src_w, dst += dst_w)
					for (i = clipped.w(), s = src, d = dst; i--; s++, d++)
						if (s->pixel) *d = *s;
				break;
			}

			_flush_pixels(clipped);
		}
};

#endif /* _INCLUDE__NITPICKER_GFX__CHUNKY_CANVAS_H_ */
