/*
 * \brief  Blitting test
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <blit/blit.h>
#include <blit/internal/slow.h>

using namespace Blit;


/*******************************
 ** Low-level SIMD operations **
 *******************************/

template <unsigned W, unsigned H>
struct Image
{
	static constexpr unsigned w = W, h = H;

	uint32_t pixels[W*H];

	void print(Output &out) const
	{
		using Genode::print;
		for (unsigned y = 0; y < H; y++) {
			for (unsigned x = 0; x < min(25u, W); x++) {
				uint32_t v = pixels[y*W+x];
				if (v)
					print(out, " ", Char('A' + (v&63)), Char(char('A' + ((v>>16)&63))));
				else
					print(out, "  .");
			}
			if (y < H-1) print(out, "\n");
		}
	}

	bool operator != (Image const &other)
	{
		for (unsigned i = 0; i < W*H; i++)
			if (other.pixels[i] != pixels[i])
				return true;
		return false;
	}

	static Image pattern()
	{
		Image image { };
		for (unsigned y = 0; y < H; y++)
			for (unsigned x = 0; x < W; x++)
				image.pixels[y*W + x] = (y << 16) | x;
		return image;
	}
};


#define TEST_LANDSCAPE(SIMD, FN, DST_W, DST_H, W, H) \
{ \
	Image<DST_W, DST_H> dst { }, ref { }; \
	Slow:: FN(ref.pixels, ref.w/8, src.pixels, W, H); \
	log(#FN, " ref:\n", ref); \
	SIMD:: FN(dst.pixels, dst.w/8, src.pixels, W, H); \
	log(#FN, " got:\n", dst); \
	if (dst != ref) { \
		error("", #FN, " failed"); \
		throw 1; \
	} \
}


#define TEST_PORTRAIT(SIMD, FN, DST_W, DST_H, W, H) \
{ \
	Image<DST_W, DST_H> dst { }, ref { }; \
	Slow:: FN(ref.pixels, ref.w/8, src.pixels, src.w/8, W, H); \
	log(#FN, " ref:\n", ref); \
	SIMD:: FN(dst.pixels, dst.w/8, src.pixels, src.w/8, W, H); \
	log(#FN, " got:\n", dst); \
	if (dst != ref) { \
		error("", #FN, " failed"); \
		throw 1; \
	} \
}


template <typename SIMD>
static void test_simd_b2f()
{
	static Image<48,32> const src = Image<48,32>::pattern();

	log("source image:\n", src);

	TEST_LANDSCAPE ( SIMD, B2f       ::r0, 48, 32, 2, 4 );
	TEST_LANDSCAPE ( SIMD, B2f_flip  ::r0, 48, 32, 2, 4 );
	TEST_PORTRAIT  ( SIMD, B2f      ::r90, 32, 48, 4, 2 );
	TEST_PORTRAIT  ( SIMD, B2f_flip ::r90, 32, 48, 4, 2 );
	TEST_LANDSCAPE ( SIMD, B2f     ::r180, 48, 32, 2, 4 );
	TEST_LANDSCAPE ( SIMD, B2f_flip::r180, 48, 32, 2, 4 );
	TEST_PORTRAIT  ( SIMD, B2f     ::r270, 32, 48, 4, 2 );
	TEST_PORTRAIT  ( SIMD, B2f_flip::r270, 32, 48, 4, 2 );
}


/****************************************
 ** Back-to-front argument dispatching **
 ****************************************/

struct Recorded
{
	struct Args
	{
		uint32_t       *dst;
		unsigned        dst_w;
		uint32_t const *src;
		unsigned        src_w;
		unsigned        w, h;

		bool operator != (Args const &other) const
		{
			return dst   != other.dst
			    || dst_w != other.dst_w
			    || src   != other.src
			    || src_w != other.src_w
			    || w     != other.w
			    || h     != other.h;
		}

		void print(Output &out) const
		{
			bool const valid = (*this != Args { });
			if (!valid) {
				Genode::print(out, "invalid");
				return;
			}

			/* print src and dst pointer values in units of uint32_t words */
			Genode::print(out, "dst=", Hex(addr_t(dst)/4), " dst_w=", dst_w,
			                  " src=", Hex(addr_t(src)/4), " src_w=", src_w, " w=", w, " h=", h);
		}
	};

	static Args recorded;

	static void _record(uint32_t       *dst, unsigned line_w,
	                    uint32_t const *src, unsigned w, unsigned h)
	{
		recorded = { dst, line_w, src, line_w, w, h };
	}

	static void _record(uint32_t       *dst, unsigned dst_w,
	                    uint32_t const *src, unsigned src_w, unsigned w, unsigned h)
	{
		recorded = { dst, dst_w, src, src_w, w, h };
	}

	struct B2f
	{
		static inline void r0    (auto &&... args) { _record(args...); }
		static inline void r90   (auto &&... args) { _record(args...); }
		static inline void r180  (auto &&... args) { _record(args...); }
		static inline void r270  (auto &&... args) { _record(args...); }
	};

	struct B2f_flip
	{
		static inline void r0  (auto &&... args) { _record(args...); }
		static inline void r90 (auto &&... args) { _record(args...); }
		static inline void r180(auto &&... args) { _record(args...); }
		static inline void r270(auto &&... args) { _record(args...); }
	};
};


Recorded::Args Recorded::recorded { };

namespace Blit {

	static inline const char *name(Rotate r)
	{
		switch (r) {
		case Rotate::R0:   return "R0";
		case Rotate::R90:  return "R90";
		case Rotate::R180: return "R180";
		case Rotate::R270: return "R270";
		}
		return "invalid";
	}
}


static inline void test_b2f_dispatch()
{
	Texture<Pixel_rgb888> texture_landscape { nullptr, nullptr, { 640, 480 } };
	Texture<Pixel_rgb888> texture_portrait  { nullptr, nullptr, { 480, 640 } };
	Surface<Pixel_rgb888> surface           { nullptr,          { 640, 480 } };

	struct Expected : Recorded::Args { };

	auto expected = [&] (addr_t dst, unsigned dst_w, addr_t src, unsigned src_w,
	                     unsigned w, unsigned h)
	{
		return Expected { (uint32_t *)(4*dst), dst_w,
		                  (uint32_t *)(4*src), src_w, w, h };
	};

	using Rect = Blit::Rect;

	auto test = [&] (Texture<Pixel_rgb888> const &texture,
	                 Rect rect, Rotate rotate, Flip flip,
	                 Expected const &expected)
	{
		Recorded::recorded = { };
		_b2f<Recorded>(surface, texture, rect, rotate, flip);
		log("b2f: ", rect, " ", name(rotate), flip.enabled ? " flip" : "",
		    " -> ", Recorded::recorded);
		if (Recorded::recorded != expected) {
			error("test_b2f_dispatch failed, expected: ", expected);
			throw 1;
		}
	};

	log("offset calculation of destination window");
	{
		unsigned const x = 32, y = 16, w = 64, h = 48;

		addr_t const src_landscape_ptr = y*640 + x,
		             src_portrait_ptr  = y*480 + x;

		Rect const rect { { x, y }, { w, h } };

		test(texture_landscape, rect, Rotate::R0,   Flip { },
		     expected(y*640 + x, 80, src_landscape_ptr, 80, 8, 6));
		test(texture_landscape, rect, Rotate::R0,   Flip { true },
		     expected(y*640 + 640 - w - x, 80, src_landscape_ptr, 80, 8, 6));
		test(texture_portrait, rect, Rotate::R90,  Flip { },
		     expected(x*640 + 640 - h - y, 80, src_portrait_ptr, 60, 8, 6));
		test(texture_portrait, rect, Rotate::R90,  Flip { true },
		     expected(x*640 + y, 80, src_portrait_ptr, 60, 8, 6));
		test(texture_landscape, rect, Rotate::R180, Flip { },
		     expected((480 - y - h)*640 + 640 - x - w, 80, src_landscape_ptr, 80, 8, 6));
		test(texture_landscape, rect, Rotate::R180, Flip { true },
		     expected((480 - y - h)*640 + x, 80, src_landscape_ptr, 80, 8, 6));
		test(texture_portrait, rect, Rotate::R270, Flip { },
		     expected((480 - x - w)*640 + y, 80, src_portrait_ptr, 60, 8, 6));
		test(texture_portrait, rect, Rotate::R270, Flip { true },
		     expected((480 - x - w)*640 + 640 - y - h, 80, src_portrait_ptr, 60, 8, 6));
	}

	log("check for compatibility of surface and texture");
	test(texture_portrait, { { }, { 16, 16 } }, Rotate::R0, Flip { },
	     expected(0, 0, 0, 0, 0, 0));

	log("clamp rect to texture size");
	test(texture_landscape, { { -99, -99 }, { 999, 999 } }, Rotate::R0, Flip { },
	     expected(0, 80, 0, 80, 80, 60));

	log("ignore out-of-bounds rect");
	test(texture_landscape, { { 1000, 0 }, { 16, 16 } }, Rotate::R0, Flip { },
	     expected(0, 0, 0, 0, 0, 0));

	/* snap to grid */
	log("snap rect argument to 8x8 grid");
	test(texture_landscape, { { 31, 63 }, { 2, 2 } }, Rotate::R0, Flip { },
	     expected(56*640 + 24, 80, 56*640 + 24, 80, 2, 2));
}


template <typename SIMD>
static inline void test_simd_blend_mix()
{
	struct Rgb : Genode::Hex
	{
		explicit Rgb(uint32_t v) : Hex(v, OMIT_PREFIX, PAD) { }
	};

	struct Mix_test
	{
		uint32_t bg, fg; uint8_t a; uint32_t expected;

		void print(Output &out) const
		{
			Genode::print(out, "bg=", Rgb(bg), " fg=", Rgb(fg), " a=", a);
		}
	};

	Mix_test mix_test[] {
		{ .bg = 0x000000, .fg = 0x000000, .a = 0,   .expected = 0x000000 },
		{ .bg = 0x000000, .fg = 0xffffff, .a = 0,   .expected = 0x000000 },
		{ .bg = 0xffffff, .fg = 0x000000, .a = 0,   .expected = 0xffffff },
		{ .bg = 0xffffff, .fg = 0xffffff, .a = 0,   .expected = 0xffffff },

		{ .bg = 0x000000, .fg = 0x000000, .a = 255, .expected = 0x000000 },
		{ .bg = 0x000000, .fg = 0xffffff, .a = 255, .expected = 0xffffff },
		{ .bg = 0xffffff, .fg = 0x000000, .a = 255, .expected = 0x000000 },
		{ .bg = 0xffffff, .fg = 0xffffff, .a = 255, .expected = 0xffffff },
	};

	for (Mix_test const &test : mix_test) {
		uint32_t slow = Slow::Blend::_mix(test.bg, test.fg, test.a);
		uint32_t simd = SIMD::Blend::_mix(test.bg, test.fg, test.a);
		if (slow == test.expected && slow == simd) {
			log("mix ", test, " -> slow=", Rgb(slow), " simd=", Rgb(simd));
		} else {
			error("mix ", test, " -> slow=", Rgb(slow), " simd=", Rgb(simd),
			      " expected=", Rgb(test.expected));
			throw 1;
		}
	}

	struct Xrgb_8x
	{
		uint32_t values[8];

		void print(Output &out) const
		{
			for (unsigned i = 0; i < 8; i++)
				Genode::print(out, (i == 0) ? "" : ".", Rgb(values[i]));
		}

		bool operator != (Xrgb_8x const &other) const
		{
			for (unsigned i = 0; i < 8; i++)
				if (values[i] != other.values[i])
					return true;
			return false;
		}
	};

	uint32_t const ca = 0xaaaaaa, cb = 0xbbbbbb, cc = 0xcccccc, cd = 0xdddddd,
	               white = 0xffffff;

	Xrgb_8x black_bg { };
	Xrgb_8x white_bg { { white, white, white, white, white, white, white, white } };

	Xrgb_8x fg       { { 0x001020, 0x405060, 0x8090a0, 0xc0d0e0, ca, cb, cc, cd } };
	uint8_t alpha[8]   { 63,       127,      191,      255     , 64, 64, 64, 64 };

	auto test_mix_8 = [&] (auto msg, Xrgb_8x &bg, Xrgb_8x const &fg,
	                       uint8_t const *alpha, Xrgb_8x const &expected)
	{
		log("fg        : ", fg);
		log("bg        : ", bg);
		SIMD::Blend::xrgb_a(bg.values, 8, fg.values, alpha);
		log(msg, " : ", bg);
		if (expected != bg) {
			error("expected ", expected);
			throw 1;
		}
	};

	test_mix_8("blackened", black_bg, fg, alpha, { {
		0x00000408, 0x00202830, 0x00606c78, 0x00c0d0e0,
		0x002b2b2b, 0x002f2f2f, 0x00333333, 0x00383838 } });

	test_mix_8("whitened ", white_bg, fg, alpha, { {
		0x00c0c4c8, 0x00a0a8b0, 0x00a0acb8, 0x00c0d0e0,
		0x00eaeaea, 0x00eeeeee, 0x00f3f3f3, 0x00f7f7f7 } });
}


void Component::construct(Genode::Env &)
{
#ifdef _INCLUDE__BLIT__INTERNAL__NEON_H_
	log("-- ARM Neon --");
	test_simd_b2f<Neon>();
	test_simd_blend_mix<Neon>();
#endif
#ifdef _INCLUDE__BLIT__INTERNAL__SSE4_H_
	log("-- SSE4 --");
	test_simd_b2f<Sse4>();
	test_simd_blend_mix<Sse4>();
#endif

	test_b2f_dispatch();

	log("--- blit test finished ---");
}
