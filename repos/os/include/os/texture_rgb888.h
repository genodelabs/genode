/*
 * \brief  Support for Genode::Texture
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__TEXTURE_RGB888_H_
#define _INCLUDE__GEMS__TEXTURE_RGB888_H_

/* Genode includes */
#include <os/texture.h>
#include <os/pixel_rgb888.h>

namespace Genode {

	template <>
	inline void
	Texture<Pixel_rgb888>::rgba(unsigned char const *rgba, unsigned len, int y)
	{
		if (len > size().w()) len = size().w();
		if (y < 0 || y >= (int)size().h()) return;

		Pixel_rgb888  *dst_pixel = pixel() + y*size().w();
		unsigned char *dst_alpha = alpha() ? alpha() + y*size().w() : 0;

		for (unsigned i = 0; i < len; i++) {

			int r = *rgba++;
			int g = *rgba++;
			int b = *rgba++;
			int a = *rgba++;

			dst_pixel[i].rgba(r, g, b);

			if (dst_alpha)
				dst_alpha[i] = min(a, 255);
		}
	}
}

#endif /* _INCLUDE__GEMS__TEXTURE_RGB888_H_ */
