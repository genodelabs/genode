/*
 * \brief  Support for Genode::Texture
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/texture.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_rgb888.h>
#include <util/dither_matrix.h>

namespace Genode {

	template <>
	void
	Texture<Pixel_rgb565>::rgba(unsigned char const *rgba, unsigned len, int y)
	{
		if (len > size().w()) len = size().w();
		if (y < 0 || y >= (int)size().h()) return;

		Pixel_rgb565  *dst_pixel = pixel() + y*size().w();
		unsigned char *dst_alpha = alpha() ? alpha() + y*size().w() : 0;

		Dither_matrix::Row dither_row = Dither_matrix::row(y);

		for (unsigned i = 0; i < len; i++) {

			int v = dither_row.value(i) >> 5;
			int r = *rgba++ + v;
			int g = *rgba++ + v;
			int b = *rgba++ + v;
			int a = *rgba++;

			if (a) a += v;

			dst_pixel[i].rgba(min(r, 255), min(g, 255), min(b, 255));

			if (dst_alpha)
				dst_alpha[i] = min(a, 255);
		}
	}


	template <>
	void
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


template class Genode::Texture<Genode::Pixel_rgb565>;
template class Genode::Texture<Genode::Pixel_rgb888>;
