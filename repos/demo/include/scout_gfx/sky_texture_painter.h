/*
 * \brief  Functor for drawing a sky texture into a surface
 * \author Norman Feske
 * \date   2005-10-24
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT_GFX__SKY_TEXTURE_PAINTER_H_
#define _INCLUDE__SCOUT_GFX__SKY_TEXTURE_PAINTER_H_

#include <os/surface.h>
#include <util/color.h>


struct Sky_texture_painter
{
	typedef Genode::Surface_base::Area Area;
	typedef Genode::Surface_base::Rect Rect;
	typedef Genode::Color              Color;


	template <typename PT>
	static void _compose(PT *dst, int dst_w, int dst_h, int x_start, int x_end,
	                     short const src1[], int src1_y,
	                     short const src2[], int src2_y,
	                     short const src3[], int src3_y, int src_w, int src_h,
	                     PT    const coltab[])
	{
		for (int k = 0; k <= x_end; k += src_w) {

			int x_offset = Genode::max(0, x_start - k);
			int x_max    = Genode::min(x_end - k, src_w - 1);

			for (int j = 0; j < dst_h; j++) {

				short const *s1 = src1 + x_offset + ((src1_y + j)%src_h)*src_w;
				short const *s2 = src2 + x_offset + ((src2_y + j)%src_h)*src_w;
				short const *s3 = src3 + x_offset + ((src3_y + j)%src_h)*src_w;
				PT          *d  = dst  + x_offset + j*dst_w  + k;

				for (int i = x_offset; i <= x_max; i++)
					*d++ = coltab[*s1++ + *s2++ + *s3++];
			}
		}
	}


	class Sky_texture_base
	{
		protected:

			virtual ~Sky_texture_base() { }

			static void _brew_texture(short tmp[], short tmp2[], short dst[], int w, int h,
			                          int lf_start, int lf_end, int lf_incr, int lf_mul,
			                          int hf_val, int hf_mul);

			/**
			 * Multiply buffer values with 24:8 fixpoint value
			 */
			static void _multiply_buf(short dst[], int len, int factor)
			{
				for (int i = 0; i < len; i++)
					dst[i] = (dst[i]*factor)>>8;
			}

			static inline int _mix_channel(int value1, int value2, int alpha)
			{
				return (value1*(255 - alpha) + value2*alpha)>>8;
			}
	};


	template <typename PT>
	class Sky_texture : public Sky_texture_base
	{
		private:

			Area _size;

		public:

			Sky_texture(Area size) : _size(size) { }

			virtual ~Sky_texture() { }

			virtual PT    const *fallback()      const = 0;
			virtual short const *buf(unsigned i) const = 0;
			virtual PT    const *coltab()        const = 0;

			Area size() const { return _size; }
	};


	/*
	 * The texture is composed of four generated 4-bit maps based on bicubic
	 * interpolation of some noise at different frequencies. At runtime, we
	 * overlay (add their values) the generated map and use the result as index
	 * of a color table.
	 */
	template <typename PT, unsigned TW, unsigned TH>
	class Static_sky_texture : public Sky_texture<PT>
	{
		private:

			short _bufs[3][TH][TW];
			short _buf[TH][TW];
			short _tmp[TH][TW];
			PT    _coltab[16*16*16];
			PT    _fallback[TH][TW];  /* fallback texture */

			using Sky_texture<PT>::_mix_channel;
			using Sky_texture<PT>::_brew_texture;
			using Sky_texture<PT>::_multiply_buf;

		public:

			/**
			 * Create 3D color table
			 */
			static void _create_coltab(PT *dst, Color c0, Color c1, Color c2, Color bg)
			{
				for (int i = 0; i < 16; i++)
					for (int j = 0; j < 16; j++)
						for (int k = 0; k < 16; k++) {

							int r = bg.r;
							int g = bg.g;
							int b = bg.b;

							r = _mix_channel(r, c2.r, k*16);
							g = _mix_channel(g, c2.g, k*16);
							b = _mix_channel(b, c2.b, k*16);

							r = _mix_channel(r, c1.r, j*16);
							g = _mix_channel(g, c1.g, j*16);
							b = _mix_channel(b, c1.b, j*16);

							r = _mix_channel(r, c0.r, i*8);
							g = _mix_channel(g, c0.g, i*8);
							b = _mix_channel(b, c0.b, i*8);

							int v = (((i ^ j ^ k)<<1) & 0xff) + 128 + 64;

							r = (r + v)>>1;
							g = (g + v)>>1;
							b = (b + v)>>1;

							v = 180;
							r = (v*r + (255 - v)*255)>>8;
							g = (v*g + (255 - v)*255)>>8;
							b = (v*b + (255 - v)*255)>>8;

							dst[(k<<8) + (j<<4) + i].rgba(r, g, b);
						}
			}



			/**
			 * Constructor
			 */
			Static_sky_texture()
			:
				Sky_texture<PT>(Area(TW, TH))
			{
				/* create nice-looking textures */
				_brew_texture(_tmp[0], _buf[0], _bufs[0][0], TW, TH, 3,  7,  1, 30, 30, 10);
				_brew_texture(_tmp[0], _buf[0], _bufs[1][0], TW, TH, 3, 16,  3, 50, 40, 30);
				_brew_texture(_tmp[0], _buf[0], _bufs[2][0], TW, TH, 5, 40, 11, 70,  0,  0);

				/* shift texture 1 to bits 4 to 7 */
				_multiply_buf(_bufs[1][0], TW*TH, 16*256);

				/* shift texture 2 to bits 8 to 11 */
				_multiply_buf(_bufs[2][0], TW*TH, 16*16*256);

				/* create color table */
				_create_coltab(_coltab, Color(255, 255, 255),
				                        Color(  0,   0,   0),
				                        Color(255, 255, 255),
				                        Color( 80,  88, 112));

				/* create fallback texture */
				_compose(_fallback[0], TW, TH, 0, TW - 1,
				         _bufs[0][0], 0, _bufs[1][0], 0, _bufs[2][0], 0,
				         TW, TH, _coltab);
			}

			PT const *fallback() const { return _fallback[0]; }

			short const *buf(unsigned i) const
			{
				if (i >= 3)
					return 0;

				return _bufs[i][0];
			}

			PT const *coltab() const { return _coltab; }
	};


	template <typename PT>
	static void _copy(PT *dst, int dst_w, int dst_h, int x_start, int x_end,
	                  PT const *src, int src_y, int src_w, int src_h)
	{
		for (int k = 0; k <= x_end; k += src_w) {

			int x_offset = Genode::max(0, x_start - k);
			int x_max    = Genode::min(x_end - k, src_w - 1);

			for (int j = 0; j < dst_h; j++) {

				PT const *s  = src  + x_offset + ((src_y + j)%src_h)*src_w;
				PT       *d  = dst  + x_offset + j*dst_w  + k;

				if (x_max - x_offset >= 0)
					Genode::memcpy(d, s, (x_max - x_offset + 1)*sizeof(PT));
			}
		}
	}


	template <typename PT>
	static inline void paint(Genode::Surface<PT> &surface, int py,
	                         Sky_texture_base const &texture_base,
	                         bool detail)
	{
		PT *addr = surface.addr();

		if (!addr) return;

		Sky_texture<PT> const &texture = static_cast<Sky_texture<PT> const &>(texture_base);

		int cx1 = surface.clip().x1();
		int cy1 = surface.clip().y1();
		int cx2 = surface.clip().x2();
		int cy2 = surface.clip().y2();

		int v  = -py;
		int y0 = cy1 + v;
		int y1 = cy1 + (( (5*v)/16)%texture.size().h());
		int y2 = cy1 + (((11*v)/16)%texture.size().h());

		addr += cy1*surface.size().w();

		if (detail == false) {
			_copy(addr, surface.size().w(), cy2 - cy1 + 1, cx1, cx2,
			      texture.fallback(), cy1 - py, texture.size().w(), texture.size().h());
			return;
		}

		_compose(addr, surface.size().w(), cy2 - cy1 + 1, cx1, cx2,
		         texture.buf(0), y0, texture.buf(1), y1, texture.buf(2), y2,
		         texture.size().w(), texture.size().h(), texture.coltab());

		surface.flush_pixels(surface.clip());
	}
};

#endif /* _INCLUDE__SCOUT_GFX__SKY_TEXTURE_PAINTER_H_ */
