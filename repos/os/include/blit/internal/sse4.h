/*
 * \brief  2D memory copy using SSE4
 * \author Norman Feske
 * \date   2025-01-21
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__SSE4_H_
#define _INCLUDE__BLIT__INTERNAL__SSE4_H_

#include <blit/types.h>

/* compiler intrinsics */
#ifndef _MM_MALLOC_H_INCLUDED   /* discharge dependency from stdlib.h */
#define _MM_MALLOC_H_INCLUDED
#define _MM_MALLOC_H_INCLUDED_PREVENTED
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <immintrin.h>
#pragma GCC diagnostic pop
#ifdef  _MM_MALLOC_H_INCLUDED_PREVENTED
#undef  _MM_MALLOC_H_INCLUDED
#undef  _MM_MALLOC_H_INCLUDED_PREVENTED
#endif


namespace Blit { struct Sse4; };


struct Blit::Sse4
{
	union Tile_4x4 { __m128i pi[4]; __m128  ps[4]; };

	struct Src_ptr4
	{
		__m128i const *p0, *p1, *p2, *p3;

		inline Src_ptr4(__m128i const *p, int const step)
		:
			p0(p), p1(p0 + step), p2(p1 + step), p3(p2 + step)
		{ }

		void incr_4(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void prefetch() const
		{
			_mm_prefetch(p0, _MM_HINT_T0); _mm_prefetch(p1, _MM_HINT_T0);
			_mm_prefetch(p2, _MM_HINT_T0); _mm_prefetch(p3, _MM_HINT_T0);
		}

		void load(Tile_4x4 &tile) const
		{
			tile.pi[0] = _mm_load_si128(p0); tile.pi[1] = _mm_load_si128(p1);
			tile.pi[2] = _mm_load_si128(p2); tile.pi[3] = _mm_load_si128(p3);
		}
	};

	struct Dst_ptr4
	{
		__m128i *p0, *p1, *p2, *p3;

		Dst_ptr4(__m128i *p, int const step_4)
		:
			p0(p), p1(p0 + step_4), p2(p1 + step_4), p3(p2 + step_4)
		{ }

		void incr_4(int v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void store(Tile_4x4 const &tile) const
		{
			_mm_stream_si128(p0, tile.pi[0]); _mm_stream_si128(p1, tile.pi[1]);
			_mm_stream_si128(p2, tile.pi[2]); _mm_stream_si128(p3, tile.pi[3]);
		}
	};

	struct Steps { int src_y_4, dst_y_4; };

	static inline void _reverse_line(__m128i const *s, __m128i *d, unsigned len_8)
	{
		static constexpr int reversed = (0 << 6) | (1 << 4) | (2 << 2) | 3;

		d += 2*len_8;   /* move 'dst' from end towards begin */

		while (len_8--) {
			__m128i const v0 = _mm_load_si128(s++);
			__m128i const v1 = _mm_load_si128(s++);
			_mm_stream_si128(--d, _mm_shuffle_epi32(v0, reversed));
			_mm_stream_si128(--d, _mm_shuffle_epi32(v1, reversed));
		}
	};

	static inline void _copy_line(__m128i const *s, __m128i *d, unsigned len_8)
	{
		while (len_8--) {
			__m128i const v0 = _mm_load_si128(s++);
			__m128i const v1 = _mm_load_si128(s++);
			_mm_stream_si128(d++, v0);
			_mm_stream_si128(d++, v1);
		}
	};

	static inline void _rotate_4_lines(Src_ptr4 src, Dst_ptr4 dst,
	                                   unsigned len_4, auto const dst_4_step)
	{
		Tile_4x4 t;
		while (len_4--) {
			src.load(t);
			src.incr_4(1);
			src.prefetch();
			_MM_TRANSPOSE4_PS(t.ps[0], t.ps[1], t.ps[2], t.ps[3]);
			dst.store(t);
			dst.incr_4(dst_4_step);
		};
	};

	static inline void _rotate(Src_ptr4 src, Dst_ptr4 dst,
	                           Steps const steps, unsigned w, unsigned h)
	{
		for (unsigned i = 2*h; i; i--) {
			_rotate_4_lines(src, dst, 2*w, 4*steps.dst_y_4);
			src.incr_4(4*steps.src_y_4);
			dst.incr_4(1);
		}
	}

	struct B2f;
	struct B2f_flip;
};


struct Blit::Sse4::B2f
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Sse4::B2f::r0(uint32_t       *dst, unsigned line_w,
                         uint32_t const *src, unsigned w, unsigned h)
{
	__m128i const *s = (__m128i const *)src;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		_copy_line(s, d, w);
		s += 2*line_w;
		d += 2*line_w;
	}
}


void Blit::Sse4::B2f::r90(uint32_t       *dst, unsigned dst_w,
                          uint32_t const *src, unsigned src_w,
                          unsigned w, unsigned h)
{
	Steps const steps { -2*int(src_w), 2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src + 2*src_w*(8*h - 1), steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst,                     steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Sse4::B2f::r180(uint32_t       *dst, unsigned line_w,
                           uint32_t const *src, unsigned w, unsigned h)
{
	__m128i       *d = (__m128i *)dst;
	__m128i const *s = (__m128i const *)src + 2*line_w*8*h;

	for (unsigned i = h*8; i; i--) {
		s -= 2*line_w;
		_reverse_line(s, d, w);
		d += 2*line_w;
	}
}


void Blit::Sse4::B2f::r270(uint32_t       *dst, unsigned dst_w,
                           uint32_t const *src, unsigned src_w,
                           unsigned w, unsigned h)
{
	Steps const steps { 2*int(src_w), -2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src,                          steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst + 2*int(dst_w)*(8*w - 1), steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


struct Blit::Sse4::B2f_flip
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Sse4::B2f_flip::r0(uint32_t       *dst, unsigned line_w,
                              uint32_t const *src, unsigned w, unsigned h)
{
	__m128i const *s = (__m128i const *)src;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		_reverse_line(s, d, w);
		s += 2*line_w;
		d += 2*line_w;
	}
}


void Blit::Sse4::B2f_flip::r90(uint32_t       *dst, unsigned dst_w,
                               uint32_t const *src, unsigned src_w,
                               unsigned w, unsigned h)
{
	Steps const steps { 2*int(src_w), 2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src, steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst, steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Sse4::B2f_flip::r180(uint32_t       *dst, unsigned line_w,
                                uint32_t const *src, unsigned w, unsigned h)
{
	__m128i const *s = (__m128i const *)src + 2*line_w*8*h;
	__m128i       *d = (__m128i       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		s -= 2*line_w;
		_copy_line(s, d, w);
		d += 2*line_w;
	}
}


void Blit::Sse4::B2f_flip::r270(uint32_t       *dst, unsigned dst_w,
                                uint32_t const *src, unsigned src_w,
                                unsigned w, unsigned h)
{
	Steps const steps { -2*int(src_w), -2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src + 2*int(src_w)*(h*8 - 1), steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst + 2*int(dst_w)*(w*8 - 1), steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}

#endif /* _INCLUDE__BLIT__INTERNAL__SSE4_H_ */
