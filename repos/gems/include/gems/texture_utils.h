/*
 * \brief  Utilities for working with textures
 * \author Norman Feske
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__TEXTURE_UTILS_H_
#define _INCLUDE__GEMS__TEXTURE_UTILS_H_

#include <os/texture.h>

template <typename PT>
static void scale(Genode::Texture<PT> const &src, Genode::Texture<PT> &dst,
                  Genode::Allocator &alloc)
{
	/* sanity check to prevent division by zero */
	if (dst.size().count() == 0)
		return;

	Genode::size_t const row_num_bytes = dst.size().w()*4;
	unsigned char *row = (unsigned char *)alloc.alloc(row_num_bytes);

	unsigned const mx = (src.size().w() << 16) / dst.size().w();
	unsigned const my = (src.size().h() << 16) / dst.size().h();

	for (unsigned y = 0, src_y = 0; y < dst.size().h(); y++, src_y += my) {

		unsigned const src_line_offset = src.size().w()*(src_y >> 16);

		PT            const *pixel_line = src.pixel() + src_line_offset;
		unsigned char const *alpha_line = src.alpha() + src_line_offset;

		unsigned char *d = row;
		for (unsigned x = 0, src_x = 0; x < dst.size().w(); x++, src_x += mx) {

			unsigned const pixel_offset = src_x >> 16;

			PT            const pixel = pixel_line[pixel_offset];
			unsigned char const alpha = alpha_line[pixel_offset];

			*d++ = pixel.r();
			*d++ = pixel.g();
			*d++ = pixel.b();
			*d++ = alpha;
		}

		dst.rgba(row, dst.size().w(), y);
	}

	alloc.free(row, row_num_bytes);
}


/*
 * \deprecated
 */
template <typename PT>
static void scale(Genode::Texture<PT> const &src, Genode::Texture<PT> &dst) __attribute__ ((deprecated));
template <typename PT>
static void scale(Genode::Texture<PT> const &src, Genode::Texture<PT> &dst)
{
	scale(src, dst, *Genode::env_deprecated()->heap());
}


template <typename SRC_PT, typename DST_PT>
static void convert_pixel_format(Genode::Texture<SRC_PT> const &src,
                                 Genode::Texture<DST_PT>       &dst,
                                 unsigned                       alpha,
                                 Genode::Allocator             &alloc)
{
	/* sanity check */
	if (src.size() != dst.size())
		return;

	Genode::size_t const row_num_bytes = dst.size().w()*4;
	unsigned char *row = (unsigned char *)alloc.alloc(row_num_bytes);

	/* shortcuts */
	unsigned const w = dst.size().w(), h = dst.size().h();

	for (unsigned y = 0, line_offset = 0; y < h; y++, line_offset += w) {

		SRC_PT        const *src_pixel = src.pixel() + line_offset;
		unsigned char const *src_alpha = src.alpha() + line_offset;

		/* fill row buffer with values from source texture */
		unsigned char *d = row;
		for (unsigned x = 0; x < w; x++, src_pixel++, src_alpha++) {

			*d++ = src_pixel->r();
			*d++ = src_pixel->g();
			*d++ = src_pixel->b();
			*d++ = (*src_alpha * alpha) >> 8;
		}

		/* assign row to destination texture */
		dst.rgba(row, w, y);
	}

	alloc.free(row, row_num_bytes);
}


/*
 * deprecated
 */
template <typename SRC_PT, typename DST_PT>
static void convert_pixel_format(Genode::Texture<SRC_PT> const &src,
                                 Genode::Texture<DST_PT>       &dst,
                                 unsigned                       alpha) __attribute__((deprecated));
template <typename SRC_PT, typename DST_PT>
static void convert_pixel_format(Genode::Texture<SRC_PT> const &src,
                                 Genode::Texture<DST_PT>       &dst,
                                 unsigned                       alpha)
{
	convert_pixel_format(src, dst, alpha, *Genode::env_deprecated()->heap());
}

#endif /* _INCLUDE__GEMS__TEXTURE_UTILS_H_ */
