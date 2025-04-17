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
	/**
	 * Padded hex output utility
	 */
	template <typename T>
	struct Phex : Hex { explicit Phex(T v) : Hex(v, OMIT_PREFIX, PAD) { } };

	/**
	 * Vector output utility
	 */
	template <typename T>
	union Vec_as
	{
		__m128i v;
		static constexpr unsigned N = 128/(8*sizeof(T));
		T u[N];

		Vec_as(__m128i v) : v(v) { }

		void print(Output &out) const
		{
			for (unsigned i = 0; i < N; i++)
				Genode::print(out, Phex(u[i]), i < (N-1) ? "." : "");
		}
	};

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

	static inline void _rotate_4_lines(Src_ptr4 src, Dst_ptr4 dst, unsigned len_4,
	                                   auto const src_4_step, auto const dst_4_step)
	{
		Tile_4x4 t;
		while (len_4--) {
			src.load(t);
			src.incr_4(src_4_step);
			src.prefetch();
			_MM_TRANSPOSE4_PS(t.ps[0], t.ps[1], t.ps[2], t.ps[3]);
			dst.store(t);
			dst.incr_4(dst_4_step);
		};
	};

	static inline void _rotate(Src_ptr4 src, Dst_ptr4 dst,
	                           Steps const steps, unsigned w, unsigned h)
	{
		for (unsigned i = 2*w; i; i--) {
			_rotate_4_lines(src, dst, 2*h, 4*steps.src_y_4, 1);
			src.incr_4(1);
			dst.incr_4(4*steps.dst_y_4);
		}
	}

	struct B2f;
	struct B2f_flip;
	struct Blend;
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
	line_w >>= 3, w >>= 3, h >>= 3;

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
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { -2*int(src_w), 2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src + 2*src_w*(8*h - 1), steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst,                     steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Sse4::B2f::r180(uint32_t       *dst, unsigned line_w,
                           uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

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
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

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
	line_w >>= 3, w >>= 3, h >>= 3;

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
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { 2*int(src_w), 2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src, steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst, steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Sse4::B2f_flip::r180(uint32_t       *dst, unsigned line_w,
                                uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

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
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { -2*int(src_w), -2*int(dst_w) };

	Src_ptr4 src_ptr4 ((__m128i *)src + 2*int(src_w)*(h*8 - 1), steps.src_y_4);
	Dst_ptr4 dst_ptr4 ((__m128i *)dst + 2*int(dst_w)*(w*8 - 1), steps.dst_y_4);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


struct Blit::Sse4::Blend
{
	static inline void xrgb_a(uint32_t *, unsigned, uint32_t const *, uint8_t const *);

	__attribute__((optimize("-O3")))
	static inline uint32_t _blend(uint32_t xrgb, unsigned alpha)
	{
		return (alpha * ((xrgb & 0xff00)    >> 8) & 0xff00)
		   | (((alpha *  (xrgb & 0xff00ff)) >> 8) & 0xff00ff);
	}

	__attribute__((optimize("-O3")))
	static inline uint32_t _mix(uint32_t bg, uint32_t fg, unsigned alpha)
	{
		return (__builtin_expect(alpha == 0, false))
		       ? bg : _blend(bg, 256 - alpha) + _blend(fg, alpha + 1);
	}

	struct Mix_masks
	{
		/* masks for distributing alpha values to 16-bit r, g, b lanes */
		__m128i const a01 = _mm_set_epi32(0x03020302, 0x03020302, 0x01000100, 0x01000100);
		__m128i const a23 = _mm_set_epi32(0x07060706, 0x07060706, 0x05040504, 0x05040504);
	};

	__attribute__((optimize("-O3")))
	static inline void _mix_4(uint32_t *, uint32_t const *, uint8_t const *, Mix_masks const);
};


__attribute__((optimize("-O3")))
void Blit::Sse4::Blend::_mix_4(uint32_t *bg, uint32_t const *fg, uint8_t const *alpha, Mix_masks const masks)
{
	uint32_t const a_u8_x4 = *(uint32_t const *)alpha;

	if (__builtin_expect(a_u8_x4 == 0, false))
		return;

	auto upper_half = [&] (__m128i const v) { return _mm_shuffle_epi32(v, 2 + (3<<2)); };

	__m128i const
		/* load four foreground pixel, background pixel, and alpha values */
		fg_u8_4x4 = _mm_loadu_si128((__m128i const *)fg),
		bg_u8_4x4 = _mm_loadu_si128((__m128i const *)bg),

		/* extract first and second pair of pixel values */
		fg01_u16_4x2 = _mm_cvtepu8_epi16(fg_u8_4x4),
		fg23_u16_4x2 = _mm_cvtepu8_epi16(upper_half(fg_u8_4x4)),
		bg01_u16_4x2 = _mm_cvtepu8_epi16(bg_u8_4x4),
		bg23_u16_4x2 = _mm_cvtepu8_epi16(upper_half(bg_u8_4x4)),

		/* prepare 4 destination and source alpha values */
		a_u16_x4  = _mm_cvtepu8_epi16(_mm_set1_epi32(a_u8_x4)),
		da_u16_x4 = _mm_sub_epi16(_mm_set1_epi16(256), a_u16_x4),
		sa_u16_x4 = _mm_add_epi16(a_u16_x4, _mm_set1_epi16(1)),

		/* mix first pixel pair */
		da01_u16_4x2 = _mm_shuffle_epi8(da_u16_x4, masks.a01),
		sa01_u16_4x2 = _mm_shuffle_epi8(sa_u16_x4, masks.a01),
		mixed01 = _mm_add_epi16(_mm_mullo_epi16(fg01_u16_4x2, sa01_u16_4x2),
		                        _mm_mullo_epi16(bg01_u16_4x2, da01_u16_4x2)),

		/* mix second pixel pair */
		da23_u16_4x2 = _mm_shuffle_epi8(da_u16_x4, masks.a23),
		sa23_u16_4x2 = _mm_shuffle_epi8(sa_u16_x4, masks.a23),
		mixed23 = _mm_add_epi16(_mm_mullo_epi16(fg23_u16_4x2, sa23_u16_4x2),
		                        _mm_mullo_epi16(bg23_u16_4x2, da23_u16_4x2)),

		result_4x4 = _mm_packus_epi16(_mm_srli_epi16(mixed01, 8),
		                              _mm_srli_epi16(mixed23, 8));

	_mm_storeu_si128((__m128i *)bg, result_4x4);
}


__attribute__((optimize("-O3")))
void Blit::Sse4::Blend::xrgb_a(uint32_t *dst, unsigned n,
                               uint32_t const *pixel, uint8_t const *alpha)
{
	Mix_masks const mix_masks { };

	for (; n > 3; n -= 4, dst += 4, pixel += 4, alpha += 4)
		_mix_4(dst, pixel, alpha, mix_masks);

	for (; n--; dst++, pixel++, alpha++)
		*dst = _mix(*dst, *pixel, *alpha);
}

#endif /* _INCLUDE__BLIT__INTERNAL__SSE3_H_ */
