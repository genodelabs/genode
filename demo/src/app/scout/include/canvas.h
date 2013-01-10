/*
 * \brief   Generic interface of graphics backend and chunky template
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CANVAS_H_
#define _CANVAS_H_

#include "color.h"
#include "font.h"


class Canvas
{
	protected:

		int _clip_x1, _clip_y1;   /* left-top of clipping area     */
		int _clip_x2, _clip_y2;   /* right-bottom of clipping area */

		int _w, _h;               /* current size of canvas */
		int _capacity;            /* max number of pixels   */

	public:

		virtual ~Canvas() { }

		/**
		 * Define clipping rectangle
		 */
		void clip(int x, int y, unsigned w, unsigned h)
		{
			/* calculate left-top and right-bottom points of clipping rectangle */
			_clip_x1 = x;
			_clip_y1 = y;
			_clip_x2 = x + w - 1;
			_clip_y2 = y + h - 1;
			
			/* check against canvas boundaries */
			if (_clip_x1 < 0) _clip_x1 = 0;
			if (_clip_y1 < 0) _clip_y1 = 0;
			if (w > 0 && _clip_x2 > _w - 1) _clip_x2 = _w - 1;
			if (h > 0 && _clip_y2 > _h - 1) _clip_y2 = _h - 1;
		}

		/**
		 * Request clipping rectangle
		 */
		int clip_x1() { return _clip_x1; }
		int clip_y1() { return _clip_y1; }
		int clip_x2() { return _clip_x2; }
		int clip_y2() { return _clip_y2; }

		int w() { return _w; }
		int h() { return _h; }

		/**
		 * Set logical size of canvas
		 */
		int set_size(int w, int h)
		{
			if (w*h > _capacity) return -1;
			_w = w;
			_h = h;
			clip(0, 0, w - 1, h - 1);
			return 0;
		}

		/**
		 * Draw filled box
		 */
		virtual void draw_box(int x, int y, int w, int h, Color c) = 0;

		/**
		 * Draw string
		 */
		virtual void draw_string(int x, int y, Font *font, Color color, const char *str, int len) = 0;

		/**
		 * Return base address
		 */
		virtual void *addr() = 0;

		/**
		 * Define base address of pixel data
		 */
		virtual void addr(void *) = 0;

		/**
		 * Anonymous texture struct
		 */
		class Texture {};

		/**
		 * Allocate texture container
		 */
		virtual Texture *alloc_texture(int w, int h) = 0;

		/**
		 * Free texture container
		 */
		virtual void free_texture(Texture *texture) = 0;

		/**
		 * Assign rgba values to texture line
		 */
		virtual void set_rgba_texture(Texture *dst, unsigned char *rgba, int len, int y) = 0;

		/**
		 * Draw texture
		 */
		virtual void draw_texture(Texture *src, int x, int y) { }
};


template <typename ST, int R_MASK, int R_SHIFT,
                       int G_MASK, int G_SHIFT,
                       int B_MASK, int B_SHIFT,
                       int A_MASK, int A_SHIFT>
class Pixel_rgba
{
	private:

		/**
		 * Shift left with positive or negative shift value
		 */
		inline int shift(int value, int shift)
		{
			return shift > 0 ? value << shift : value >> -shift;
		}

	public:

		static const int r_mask = R_MASK, r_shift = R_SHIFT;
		static const int g_mask = G_MASK, g_shift = G_SHIFT;
		static const int b_mask = B_MASK, b_shift = B_SHIFT;
		static const int a_mask = A_MASK, a_shift = A_SHIFT;

		ST pixel;

		/**
		 * Constructors
		 */
		Pixel_rgba() {}

		Pixel_rgba(int red, int green, int blue, int alpha = 255)
		{
			rgba(red, green, blue, alpha);
		}

		/**
		 * Assign new rgba values
		 */
		void rgba(int red, int green, int blue, int alpha = 255)
		{
			pixel = (shift(red,   r_shift) & r_mask)
			      | (shift(green, g_shift) & g_mask)
			      | (shift(blue,  b_shift) & b_mask)
			      | (shift(alpha, a_shift) & a_mask);
		}

		inline int r() { return shift(pixel & r_mask, -r_shift); }
		inline int g() { return shift(pixel & g_mask, -g_shift); }
		inline int b() { return shift(pixel & b_mask, -b_shift); }

		/**
		 * Multiply pixel with alpha value
		 */
		static inline Pixel_rgba blend(Pixel_rgba pixel, int alpha);

		/**
		 * Mix two pixels at the ratio specified as alpha
		 */
		static inline Pixel_rgba mix(Pixel_rgba p1, Pixel_rgba p2, int alpha);

		/**
		 * Compute average color value of two pixels
		 */
		static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2);

		/**
		 * Compute average color value of four pixels
		 */
		static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2,
		                             Pixel_rgba p3, Pixel_rgba p4)
		{
			return avr(avr(p1, p2), avr(p3, p4));
		}
} __attribute__((packed));


template <typename PT>
class Chunky_canvas : public Canvas
{
	protected:

		PT *_addr;   /* base address of pixel buffer   */

		/**
		 * Utilities
		 */
		static inline int min(int a, int b) { return a < b ? a : b; }
		static inline int max(int a, int b) { return a > b ? a : b; }

	public:

		/**
		 * Initialize canvas
		 */
		void init(PT *addr, long capacity)
		{
			_addr     = addr;
			_capacity = capacity;
			_w = _h   = 0;
			_clip_x1  = _clip_y1 = 0;
			_clip_x2  = _clip_y2 = 0;
		}


		/****************************************
		 ** Implementation of Canvas interface **
		 ****************************************/

		void draw_box(int x1, int y1, int w, int h, Color color)
		{
			int x2 = x1 + w - 1;
			int y2 = y1 + h - 1;

			/* check clipping */
			if (x1 < _clip_x1) x1 = _clip_x1;
			if (y1 < _clip_y1) y1 = _clip_y1;
			if (x2 > _clip_x2) x2 = _clip_x2;
			if (y2 > _clip_y2) y2 = _clip_y2;

			if ((x1 > x2) || (y1 > y2)) return;

			PT pix(color.r, color.g, color.b);
			PT *dst, *dst_line = _addr + _w*y1 + x1;

			int alpha = color.a;

			/*
			 * ???
			 *
			 * Why can dst not be declared in the head of the inner for loop?
			 * Can I use the = operator for initializing a Pixel with a Color?
			 *
			 * ???
			 */

			if (alpha == Color::OPAQUE)
				for (h = y2 - y1 + 1; h--; dst_line += _w)
					for (dst = dst_line, w = x2 - x1 + 1; w--; dst++)
						*dst = pix;

			else if (alpha != Color::TRANSPARENT)
				for (h = y2 - y1 + 1; h--; dst_line += _w)
					for (dst = dst_line, w = x2 - x1 + 1; w--; dst++)
						*dst = PT::mix(*dst, pix, alpha);
		}

		void draw_string(int x, int y, Font *font, Color color, const char *sstr, int len)
		{
			const unsigned char *str = (const unsigned char *)sstr;

			if (!str || !font) return;

			unsigned char *src = font->img;
			int d, h = font->img_h;

			/* check top clipping */
			if ((d = _clip_y1 - y) > 0) {
				src += d*font->img_w;
				y   += d;
				h   -= d;
			}

			/* check bottom clipping */
			if ((d = y + h -1 - _clip_y2) > 0)
				h -= d;

			if (h < 1) return;

			/* skip hidden glyphs */
			for ( ; *str && len && (x + font->wtab[*str] < _clip_x1); len--)
				x += font->wtab[*str++];

			PT *dst = _addr + y*_w;
			PT pix(color.r, color.g, color.b);
			int alpha = color.a;

			/* draw glyphs */
			for ( ; *str && len && (x <= _clip_x2); str++, len--) {

				int            w     = font->wtab[*str];
				int            start = max(0, _clip_x1 - x);
				int            end   = min(w - 1, _clip_x2 - x);
				PT            *d     = dst + x;
				unsigned char *s     = src + font->otab[*str];

				for (int j = 0; j < h; j++, s += font->img_w, d += _w)
					for (int i = start; i <= end; i++)
						if (s[i]) d[i] = PT::mix(d[i], pix, (alpha*s[i]) >> 8);

				x += w;
			}
		}

		void *addr() { return _addr; }
		void  addr(void *addr) { _addr = static_cast<PT *>(addr); }

		Texture *alloc_texture(int w, int h);
		void free_texture(Texture *texture);
		void set_rgba_texture(Texture *dst, unsigned char *rgba, int len, int y);
		void draw_texture(Texture *src, int x, int y);
};

#endif /* _CANVAS_H_ */
