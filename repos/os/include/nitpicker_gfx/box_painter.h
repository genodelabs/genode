/*
 * \brief  Functor for drawing filled boxes into a surface
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_GFX__BOX_PAINTER_H_
#define _INCLUDE__NITPICKER_GFX__BOX_PAINTER_H_

#include <os/surface.h>


struct Box_painter
{
	typedef Genode::Surface_base::Rect Rect;

	/**
	 * Draw filled box
	 *
	 * \param rect   position and size of box
	 * \param color  drawing color
	 */
	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface,
	                         Rect                 rect,
	                         Genode::Color        color)
	{
		Rect clipped = Rect::intersect(surface.clip(), rect);

		if (!clipped.valid()) return;

		PT pix(color.r, color.g, color.b);
		PT *dst, *dst_line = surface.addr() + surface.size().w()*clipped.y1() + clipped.x1();

		int const alpha = color.a;

		if (color.opaque())
			for (int w, h = clipped.h() ; h--; dst_line += surface.size().w())
				for (dst = dst_line, w = clipped.w(); w--; dst++)
					*dst = pix;

		else if (!color.transparent())
			for (int w, h = clipped.h() ; h--; dst_line += surface.size().w())
				for (dst = dst_line, w = clipped.w(); w--; dst++)
					*dst = PT::mix(*dst, pix, alpha);

		surface.flush_pixels(clipped);
	}
};

#endif /* _INCLUDE__NITPICKER_GFX__BOX_PAINTER_H_ */
