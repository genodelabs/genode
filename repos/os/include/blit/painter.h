/*
 * \brief  Functor for blitting textures on a surface
 * \author Norman Feske
 * \date   2020-07-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__PAINTER_H_
#define _INCLUDE__BLIT__PAINTER_H_

#include <blit/blit.h>
#include <os/texture.h>


struct Blit_painter
{
	typedef Genode::Surface_base::Point Point;
	typedef Genode::Surface_base::Rect  Rect;


	template <typename PT>
	static inline void paint(Genode::Surface<PT>       &surface,
	                         Genode::Texture<PT> const &texture,
	                         Point                      position)
	{
		Rect const clipped = Rect::intersect(Rect(position, texture.size()),
		                                     surface.clip());

		if (!clipped.valid())
			return;

		int const src_w = texture.size().w();
		int const dst_w = surface.size().w();

		/* calculate offset of first texture pixel to copy */
		unsigned long const tex_start_offset = (clipped.y1() - position.y())*src_w
		                                     +  clipped.x1() - position.x();

		/* start address of source pixels */
		PT const * const src = texture.pixel() + tex_start_offset;

		/* start address of destination pixels */
		PT * const dst = surface.addr() + clipped.y1()*dst_w + clipped.x1();

		blit(src, src_w*sizeof(PT),
		     dst, dst_w*sizeof(PT),
		     clipped.w()*sizeof(PT), clipped.h());

		surface.flush_pixels(clipped);
	}
};

#endif /* _INCLUDE__BLIT__PAINTER_H_ */
