/*
 * \brief  Fallback 2D memory copy
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__SLOW_H_
#define _INCLUDE__BLIT__INTERNAL__SLOW_H_

#include <blit/types.h>

namespace Blit {

	struct Slow;

	static inline void _write_line(uint32_t const *src, uint32_t *dst,
	                               unsigned len, int dst_step)
	{
		for (; len--; dst += dst_step)
			*dst = *src++;
	}

	static inline void _write_lines(uint32_t const *src, unsigned src_w,
	                                uint32_t       *dst,
	                                unsigned w, unsigned h, int dx, int dy)
	{
		for (unsigned lines = h*8; lines; lines--) {
			_write_line(src, dst, 8*w, dx);
			src += 8*src_w;
			dst += dy;
		}
	};
}


struct Blit::Slow
{
	struct B2f;
	struct B2f_flip;
};


struct Blit::Slow::B2f
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Slow::B2f::r0(uint32_t       *dst, unsigned line_w,
                         uint32_t const *src, unsigned w, unsigned h)
{
	_write_lines(src, line_w, dst, w, h, 1, 8*line_w);
}


void Blit::Slow::B2f::r90(uint32_t       *dst, unsigned dst_w,
                          uint32_t const *src, unsigned src_w,
                          unsigned w, unsigned h)
{
	_write_lines(src, src_w, dst + 8*h - 1, w, h, 8*dst_w, -1);
}


void Blit::Slow::B2f::r180(uint32_t       *dst, unsigned line_w,
                           uint32_t const *src, unsigned w, unsigned h)
{
	dst += 8*w - 1 + (8*h - 1)*8*line_w;
	_write_lines(src, line_w, dst, w, h, -1, -8*line_w);
}


void Blit::Slow::B2f::r270(uint32_t       *dst, unsigned dst_w,
                           uint32_t const *src, unsigned src_w,
                           unsigned w, unsigned h)
{
	dst += 8*dst_w*(8*w - 1);
	_write_lines(src, src_w, dst, w, h, -8*dst_w, 1);
}


struct Blit::Slow::B2f_flip
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Slow::B2f_flip::r0(uint32_t       *dst, unsigned line_w,
                              uint32_t const *src, unsigned w, unsigned h)
{
	_write_lines(src, line_w, dst + 8*w - 1, w, h, -1, 8*line_w);
}


void Blit::Slow::B2f_flip::r90(uint32_t       *dst, unsigned dst_w,
                               uint32_t const *src, unsigned src_w,
                               unsigned w, unsigned h)
{
	_write_lines(src, src_w, dst, w, h, 8*dst_w, 1);
}


void Blit::Slow::B2f_flip::r180(uint32_t       *dst, unsigned line_w,
                                uint32_t const *src, unsigned w, unsigned h)
{
	dst += (8*h - 1)*8*line_w;
	_write_lines(src, line_w, dst, w, h, 1, -8*line_w);
}


void Blit::Slow::B2f_flip::r270(uint32_t       *dst, unsigned dst_w,
                                uint32_t const *src, unsigned src_w,
                                unsigned w, unsigned h)
{
	dst += 8*h - 1 + 8*dst_w*(8*w - 1);
	_write_lines(src, src_w, dst, w, h, -8*dst_w, -1);
}

#endif /* _INCLUDE__BLIT__INTERNAL__SLOW_H_ */
