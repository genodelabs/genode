/*
 * \brief  Functor for drawing text into a surface
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_
#define _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_

#include <base/stdint.h>
#include <os/surface.h>


struct Text_painter
{
	class Font
	{
		private:

			typedef Genode::int32_t int32_t;
			typedef Genode::size_t  size_t;

		public:

			unsigned char const *img;            /* font image         */
			int           const  img_w, img_h;   /* size of font image */
			int32_t       const *otab;           /* offset table       */
			int32_t       const *wtab;           /* width table        */

			/**
			 * Construct font from a TFF data block
			 */
			Font(const char *tff)
			:
				img((unsigned char *)(tff + 2056)),

				img_w(*((int32_t *)(tff + 2048))),
				img_h(*((int32_t *)(tff + 2052))),

				otab((int32_t *)(tff)),
				wtab((int32_t *)(tff + 1024))
			{ }

			/**
			 * Calculate width of string when printed with the font
			 */
			int str_w(const char *sstr, size_t len = ~0UL) const
			{
				const unsigned char *str = (const unsigned char *)sstr;
				int res = 0;
				for (; str && *str && len; len--, str++) res += wtab[*str];
				return res;
			}

			/**
			 * Calculate height of string when printed with the font
			 */
			int str_h(const char *str, size_t len = ~0UL) const { return img_h; }
	};


	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Area  Area;
	typedef Genode::Surface_base::Rect  Rect;


	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface,
	                         Point                p,
	                         Font const          &font,
	                         Genode::Color        color,
	                         char const          *sstr)
	{
		unsigned char const *str = (unsigned char const *)sstr;
		int x = p.x(), y = p.y();

		unsigned char const *src = font.img;
		int d, h = font.img_h;

		/* check top clipping */
		if ((d = surface.clip().y1() - y) > 0) {
			src += d*font.img_w;
			y   += d;
			h   -= d;
		}

		/* check bottom clipping */
		if ((d = y + h -1 - surface.clip().y2()) > 0)
			h -= d;

		if (h < 1) return;

		/* skip hidden glyphs */
		for ( ; *str && (x + font.wtab[*str] < surface.clip().x1()); )
			x += font.wtab[*str++];

		int const x_start = x;

		PT *dst = surface.addr() + y*surface.size().w();

		PT  const pix(color.r, color.g, color.b);
		int const alpha = color.a;

		/* draw glyphs */
		for ( ; *str && (x <= surface.clip().x2()); str++) {

			int const w     = font.wtab[*str];
			int const start = Genode::max(0,     surface.clip().x1() - x);
			int const end   = Genode::min(w - 1, surface.clip().x2() - x);

			PT                  *d = dst + x;
			unsigned char const *s = src + font.otab[*str];

			for (int j = 0; j < h; j++, s += font.img_w, d += surface.size().w())
				for (int i = start; i <= end; i++)
					if (s[i])
						d[i] = (s[i] == 255 && alpha == 255)
						     ? pix : PT::mix(d[i], pix, (alpha*s[i]) >> 8);
			x += w;
		}

		surface.flush_pixels(Rect(Point(x_start, y), Area(x - x_start + 1, h)));
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__TEXT_PAINTER_H_ */
