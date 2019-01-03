/*
 * \brief  Functor for painting textures on a surface
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__TEXTURE_PAINTER_H_
#define _INCLUDE__NITPICKER_GFX__TEXTURE_PAINTER_H_

#include <blit/blit.h>
#include <os/texture.h>


struct Texture_painter
{
	/*
	 * Modes for drawing textures
	 *
	 * The solid mode is used for normal operation in Nitpicker's
	 * flat mode and corresponds to plain pixel blitting. The
	 * masked mode allows us to tint texture with a specified
	 * mixing color. This feature is used by the X-Ray and Kill
	 * mode. The masked mode leaves all pixels untouched for
	 * which the corresponding texture pixel equals the mask key
	 * color (we use black). We use this mode for painting
	 * the mouse cursor.
	 */
	enum Mode {
		SOLID  = 0,   /* draw texture pixel              */
		MIXED  = 1,   /* mix texture pixel and color 1:1 */
		MASKED = 2,   /* skip pixels with mask color     */
	};


	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;


	template <typename PT>
	static inline void paint(Genode::Surface<PT>       &surface,
	                         Genode::Texture<PT> const &texture,
	                         Genode::Color              mix_color,
	                         Point                      position,
	                         Mode                       mode,
	                         bool                       allow_alpha)
	{
		Rect clipped = Rect::intersect(Rect(position, texture.size()),
		                               surface.clip());

		if (!clipped.valid()) return;

		int const src_w = texture.size().w();
		int const dst_w = surface.size().w();

		/* calculate offset of first texture pixel to copy */
		unsigned long tex_start_offset = (clipped.y1() - position.y())*src_w
		                               +  clipped.x1() - position.x();

		/* start address of source pixels */
		PT const *src = texture.pixel() + tex_start_offset;

		/* start address of source alpha values */
		unsigned char const *alpha = texture.alpha() + tex_start_offset;

		/* start address of destination pixels */
		PT *dst = surface.addr() + clipped.y1()*dst_w + clipped.x1();

		PT const mix_pixel(mix_color.r, mix_color.g, mix_color.b);

		int i, j;
		PT            const *s;
		PT                  *d;
		unsigned char const *a;

		switch (mode) {

		case SOLID:

			/*
			 * If the texture has no alpha channel, we can use
			 * a plain pixel blit.
			 */
			if (texture.alpha() == 0 || !allow_alpha) {
				blit(src, src_w*sizeof(PT),
				     dst, dst_w*sizeof(PT),
				     clipped.w()*sizeof(PT), clipped.h());
				break;
			}

			/*
			 * Copy texture with alpha blending
			 */
			for (j = clipped.h(); j--; src += src_w, alpha += src_w, dst += dst_w)
				for (i = clipped.w(), s = src, a = alpha, d = dst; i--; s++, d++, a++) {
					unsigned char const alpha_value = *a;
					if (__builtin_expect(alpha_value != 0, true))
						*d = PT::mix(*d, *s, alpha_value + 1);
				}
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

		surface.flush_pixels(clipped);
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__TEXTURE_PAINTER_H_ */
