/*
 * \brief  2D memory copy using ARM NEON
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__INTERNAL__NEON_H_
#define _INCLUDE__BLIT__INTERNAL__NEON_H_

#include <blit/types.h>

/* compiler intrinsics */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <arm_neon.h>
#pragma GCC diagnostic pop


namespace Blit { struct Neon; }


struct Blit::Neon
{
	/**
	 * Helper for printing the raw lower 64 bits of a vector via Genode::Output
	 */
	template <typename T> union Printable
	{
		Genode::uint64_t u64; T vec;
		Printable(T vec) : vec(vec) { }
		void print(Output &out) const { Genode::print(out, Hex(u64)); }
	};

	static inline uint32x4_t _reversed(uint32x4_t const v)
	{
		return vrev64q_u32(vcombine_u32(vget_high_u32(v), vget_low_u32(v)));
	}

	static inline void _reverse_line(uint32x4_t const *src, uint32x4_t *dst, unsigned len)
	{
		src += len;   /* move 'src' from end of line towards begin */
		while (len--)
			*dst++ = _reversed(*--src);
	};

	static inline void _copy_line(uint32x4_t const *s, uint32x4_t *d, unsigned len)
	{
		while (len--)
			*d++ = *s++;
	};

	struct Src_ptr4
	{
		uint32x4_t const *p0, *p1, *p2, *p3;

		inline Src_ptr4(uint32x4_t const *p, int const step)
		:
			p0(p), p1(p0 + step), p2(p1 + step), p3(p2 + step)
		{ }

		void incr_4(int const v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void prefetch() const
		{
			__builtin_prefetch(p0); __builtin_prefetch(p1);
			__builtin_prefetch(p2); __builtin_prefetch(p3);
		}

		void load(uint32x4x4_t &tile) const { tile = { *p0, *p1, *p2, *p3 }; }
	};

	struct Dst_ptr4
	{
		uint32_t *p0, *p1, *p2, *p3;

		Dst_ptr4(uint32_t *p, int const step)
		:
			p0(p), p1(p0 + step), p2(p1 + step), p3(p2 + step)
		{ }

		void incr(int const v) { p0 += v, p1 += v, p2 += v, p3 += v; }

		void store(uint32x4x4_t const &tile)
		{
			vst4q_lane_u32(p0, tile, 0);
			vst4q_lane_u32(p1, tile, 1);
			vst4q_lane_u32(p2, tile, 2);
			vst4q_lane_u32(p3, tile, 3);
		}
	};

	struct Steps
	{
		int const src_y, dst_y;

		void incr_x_4(Src_ptr4 &p) const { p.incr_4(1); };
		void incr_x_8(Src_ptr4 &p) const { p.incr_4(2); };
		void incr_y_4(Src_ptr4 &p) const { p.incr_4(src_y << 2); };
		void incr_y_8(Src_ptr4 &p) const { p.incr_4(src_y << 3); };

		void incr_x_4(Dst_ptr4 &p) const { p.incr(4); };
		void incr_x_8(Dst_ptr4 &p) const { p.incr(8); };
		void incr_y_4(Dst_ptr4 &p) const { p.incr(dst_y << 2); };
		void incr_y_8(Dst_ptr4 &p) const { p.incr(dst_y << 3); };
	};

	__attribute__((optimize("-O3")))
	static inline void _load_prefetch_store(Src_ptr4 &src, Dst_ptr4 &dst, Steps const steps)
	{
		uint32x4x4_t tile;
		src.load(tile);
		steps.incr_y_4(src);
		src.prefetch();
		dst.store(tile);
		steps.incr_x_4(dst);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_8x4(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		for (unsigned i = 0; i < 2; i++)
			_load_prefetch_store(src, dst, steps);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_8x4_last(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		_load_prefetch_store(src, dst, steps);
		uint32x4x4_t tile;
		src.load(tile);
		dst.store(tile);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_8x8(Src_ptr4 src, Dst_ptr4 dst, Steps const steps)
	{
		_rotate_8x4(src, dst, steps);
		steps.incr_y_4(dst);
		steps.incr_x_4(src);
		_rotate_8x4_last(src, dst, steps);
	}

	__attribute__((optimize("-O3")))
	static inline void _rotate_8_lines(Src_ptr4 src, Dst_ptr4 dst,
	                                   Steps const steps, unsigned n)
	{
		for (; n; n--) {
			_rotate_8x8(src, dst, steps);
			steps.incr_y_8(dst);
			steps.incr_x_8(src);
		}
	};

	static inline void _rotate(Src_ptr4 src, Dst_ptr4 dst,
	                           Steps const steps, unsigned w, unsigned h)
	{
		for (unsigned i = h; i; i--) {
			_rotate_8_lines(src, dst, steps, w);
			steps.incr_y_8(src);
			steps.incr_x_8(dst);
		}
	}

	struct B2f;
	struct B2f_flip;
	struct Blend;
};


struct Blit::Neon::B2f
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Neon::B2f::r0(uint32_t       *dst, unsigned line_w,
                         uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

	uint32x4_t const *s = (uint32x4_t const *)src;
	uint32x4_t       *d = (uint32x4_t       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		_copy_line(s, d, 2*w);
		s += 2*line_w;
		d += 2*line_w;
	}
}


void Blit::Neon::B2f::r90(uint32_t       *dst, unsigned dst_w,
                          uint32_t const *src, unsigned src_w,
                          unsigned w, unsigned h)
{
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { -2*int(src_w), 8*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src + 2*src_w*(8*h - 1), steps.src_y);
	Dst_ptr4 dst_ptr4 (dst,                                   steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Neon::B2f::r180(uint32_t       *dst, unsigned line_w,
                           uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

	uint32x4_t       *d = (uint32x4_t *)dst;
	uint32x4_t const *s = (uint32x4_t const *)src + 2*line_w*8*h;

	for (unsigned i = h*8; i; i--) {
		s -= 2*line_w;
		_reverse_line(s, d, 2*w);
		d += 2*line_w;
	}
}


void Blit::Neon::B2f::r270(uint32_t       *dst, unsigned dst_w,
                           uint32_t const *src, unsigned src_w,
                           unsigned w, unsigned h)
{
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { 2*int(src_w), -8*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src,            steps.src_y);
	Dst_ptr4 dst_ptr4 (dst + 8*int(dst_w)*(w*8 - 1), steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


struct Blit::Neon::B2f_flip
{
	static inline void r0  (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r90 (uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
	static inline void r180(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned);
	static inline void r270(uint32_t *, unsigned, uint32_t const *, unsigned, unsigned, unsigned);
};


void Blit::Neon::B2f_flip::r0(uint32_t       *dst, unsigned line_w,
                              uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

	uint32x4_t const *s = (uint32x4_t const *)src;
	uint32x4_t       *d = (uint32x4_t       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		_reverse_line(s, d, 2*w);
		s += 2*line_w;
		d += 2*line_w;
	}
}


void Blit::Neon::B2f_flip::r90(uint32_t       *dst, unsigned dst_w,
                               uint32_t const *src, unsigned src_w,
                               unsigned w, unsigned h)
{
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { 2*int(src_w), 8*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src, steps.src_y);
	Dst_ptr4 dst_ptr4 (dst,               steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


void Blit::Neon::B2f_flip::r180(uint32_t       *dst, unsigned line_w,
                                uint32_t const *src, unsigned w, unsigned h)
{
	line_w >>= 3, w >>= 3, h >>= 3;

	uint32x4_t const *s = (uint32x4_t const *)src + 2*line_w*8*h;
	uint32x4_t       *d = (uint32x4_t       *)dst;

	for (unsigned lines = h*8; lines; lines--) {
		s -= 2*line_w;
		_copy_line(s, d, 2*w);
		d += 2*line_w;
	}
}


void Blit::Neon::B2f_flip::r270(uint32_t       *dst, unsigned dst_w,
                                uint32_t const *src, unsigned src_w,
                                unsigned w, unsigned h)
{
	dst_w >>= 3, src_w >>= 3, w >>= 3, h >>= 3;

	Steps const steps { -2*int(src_w), -8*int(dst_w) };

	Src_ptr4 src_ptr4 ((uint32x4_t *)src + 2*src_w*(8*h - 1), steps.src_y);
	Dst_ptr4 dst_ptr4 (dst + 8*int(dst_w)*(w*8 - 1),          steps.dst_y);

	_rotate(src_ptr4, dst_ptr4, steps, w, h);
}


struct Blit::Neon::Blend
{
	static inline void xrgb_a(uint32_t *, unsigned, uint32_t const *, uint8_t const *);

	__attribute__((optimize("-O3")))
	static inline uint32_t _mix(uint32_t bg, uint32_t fg, uint8_t alpha)
	{
		if (__builtin_expect(alpha == 0, false))
			return bg;

		/*
		 * Compute r, g, b in the lower 3 16-bit lanes.
		 * The upper 5 lanes are unused.
		 */
		uint16x8_t const
			a   = vmovl_u8(vdup_n_u8(alpha)),
			s   = vmovl_u8(vcreate_u8(fg)),
			d   = vmovl_u8(vcreate_u8(bg)),
			ar  = vaddq_u16(vdupq_n_u16(1),   a),  /* for rounding up */
			nar = vsubq_u16(vdupq_n_u16(256), a),  /* 1.0 - alpha */
			res = vaddq_u16(vmulq_u16(s, ar), vmulq_u16(d, nar));

		return uint32_t(::uint64_t(vshrn_n_u16(res, 8)));
	}

	__attribute__((optimize("-O3")))
	static inline void _mix_8(uint32_t *bg, uint32_t const *fg, uint8_t const *alpha)
	{
		/* fetch 8 alpha values */
		uint16x8_t const a = vmovl_u8(*(uint8x8_t *)alpha);

		/* skip block if entirely transparent */
		if (__builtin_expect(vmaxvq_u16(a) == 0, false))
			return;

		/* load 8 source and destination pixels */
		uint8x8x4_t const s = vld4_u8((uint8_t const *)fg);
		uint8x8x4_t       d = vld4_u8((uint8_t const *)bg);

		/* extend r, g, b components from uint8_t to uint16_t */
		uint16x8x4_t const
			s_rgb { vmovl_u8(s.val[0]), vmovl_u8(s.val[1]), vmovl_u8(s.val[2]) },
			d_rgb { vmovl_u8(d.val[0]), vmovl_u8(d.val[1]), vmovl_u8(d.val[2]) };

		/* load 8 alpha values, prepare as factors for source and destination */
		uint16x8_t const
			sa = vaddq_u16(vdupq_n_u16(1),   a),
			da = vsubq_u16(vdupq_n_u16(256), a);  /* 1.0 - alpha */

		/* mix components, keeping only their upper 8 bits */
		for (unsigned i = 0; i < 3; i++)
			d.val[i] = vshrn_n_u16(vaddq_u16(vmulq_u16(d_rgb.val[i], da),
			                                 vmulq_u16(s_rgb.val[i], sa)), 8);
		/* write 8 pixels */
		vst4_u8((uint8_t *)bg, d);
	}
};


__attribute__((optimize("-O3")))
void Blit::Neon::Blend::xrgb_a(uint32_t *dst, unsigned n,
                               uint32_t const *pixel, uint8_t const *alpha)
{
	int const prefetch_distance = 16;  /* cache line / 32-bit pixel size */
	for (; n > prefetch_distance; n -= 8, dst += 8, pixel += 8, alpha += 8) {
		__builtin_prefetch(dst   + prefetch_distance);
		__builtin_prefetch(pixel + prefetch_distance);
		__builtin_prefetch(alpha + prefetch_distance);
		_mix_8(dst, pixel, alpha);
	}

	for (; n > 7; n -= 8, dst += 8, pixel += 8, alpha += 8)
		_mix_8(dst, pixel, alpha);

	for (; n--; dst++, pixel++, alpha++)
		*dst = _mix(*dst, *pixel, *alpha);
}

#endif /* _INCLUDE__BLIT__INTERNAL__NEON_H_ */
