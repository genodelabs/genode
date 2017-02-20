/*
 * \brief  Functor for drawing a horizonatal shadow onto a surface
 * \author Norman Feske
 * \date   2005-10-24
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT_GFX__HORIZONTAL_SHADOW_PAINTER_H_
#define _INCLUDE__SCOUT_GFX__HORIZONTAL_SHADOW_PAINTER_H_

#include <os/surface.h>

struct Horizontal_shadow_painter
{
	typedef Genode::Surface_base::Rect Rect;

	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface, Rect rect,
	                         int intensity)
	{
		PT *addr = surface.addr();

		if (!addr) return;

		const int cx1 = surface.clip().x1();
		const int cy1 = surface.clip().y1();
		const int cx2 = surface.clip().x2();
		const int cy2 = surface.clip().y2();

		int x = rect.x1();
		int y = rect.y1();
		int w = rect.w();
		int h = rect.h();

		int curr_a = intensity;
		int step   = rect.h() ? (curr_a/rect.h()) : 0;

		if (x < cx1) {
			w -= cx1 - x;
			x = cx1;
		}

		if (y < cy1) {
			h      -=  cy1 - y;
			curr_a -= (cy1 - y)*step;
			y       = cy1;
		}

		if (w > cx2 - x + 1)
			w = cx2 - x + 1;

		if (h > cy2 - y + 1)
			h = cy2 - y + 1;

		addr += surface.size().w()*y + x;

		PT shadow_color(0,0,0);

		for (int j = 0; j < h; j++, addr += surface.size().w()) {

			PT *d = addr;

			for (int i = 0; i < w; i++, d++)
				*d = PT::mix(*d, shadow_color, curr_a);

			curr_a -= step;
		}
	}
};

#endif /* _INCLUDE__SCOUT_GFX__HORIZONTAL_SHADOW_PAINTER_H_ */
