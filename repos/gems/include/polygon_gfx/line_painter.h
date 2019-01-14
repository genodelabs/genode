/*
 * \brief  Functor for drawing lines on a surface
 * \author Norman Feske
 * \date   2017-07-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__POLYGON_GFX__LINE_PAINTER_H_
#define _INCLUDE__POLYGON_GFX__LINE_PAINTER_H_

#include <os/surface.h>
#include <util/bezier.h>


struct Line_painter
{
	typedef Genode::Surface_base::Rect  Rect;
	typedef Genode::Surface_base::Point Point;

	/**
	 * Fixpoint number with 8 fractional bits
	 */
	class Fixpoint
	{
		public:

			enum { FRAC_BITS = 8, FRAC_MASK = (1 << FRAC_BITS) - 1 };

			long const value;

		private:

			Fixpoint(long v) : value(v) { }

		public:

			static Fixpoint from_int(long v) { return Fixpoint(v << FRAC_BITS); }
			static Fixpoint from_raw(long v) { return Fixpoint(v); }

			long integer()    const { return value >> FRAC_BITS; }
			long fractional() const { return value &  FRAC_MASK; }
	};

	/**
	 * Look-up table used for the non-linear application of alpha values
	 */
	struct Lut
	{
		unsigned char value[255];

		Lut()
		{
			auto fill_segment = [&] (long x1, long y1, long x2, long)
			{
				for (long i = x1; i < x2; i++) value[i] = y1;
			};

			int const v = 210;
			bezier(0, 0, 255 - v, v, 255, 255, fill_segment, 6);
		}
	};

	/*
	 * Use a single 'Lut' object across multiple instances of 'Line_painter'.
	 */
	static Lut const &_init_lut() { static Lut lut; return lut; }

	Lut const &_lut = _init_lut();

	template <typename PT>
	static void _mix_pixel(PT &dst, PT src, int alpha)
	{
		dst = PT::mix(dst, src, alpha);
	}

	template <typename PT>
	void _transfer_pixel(PT *dst, unsigned dst_w, PT pixel, int alpha,
	                     Fixpoint x, Fixpoint y) const
	{
		dst += y.integer()*dst_w + x.integer();

		/*
		 * The values of 'u' and 'v' correspond to the subpixel position
		 * of the coordinate (x, y) within its surrounding four pixels.
		 * They are used to weight the transfer of the color value to
		 * those pixels.
		 */
		unsigned long const u = x.fractional(), inv_u = 255 - u,
		                    v = y.fractional(), inv_v = 255 - v;

		_mix_pixel(dst[0],         pixel, _lut.value[(alpha * inv_u * inv_v) >> 16]);
		_mix_pixel(dst[1],         pixel, _lut.value[(alpha *     u * inv_v) >> 16]);
		_mix_pixel(dst[dst_w],     pixel, _lut.value[(alpha * inv_u *     v) >> 16]);
		_mix_pixel(dst[dst_w + 1], pixel, _lut.value[(alpha *     u *     v) >> 16]);
	}

	/**
	 * Draw line with sub-pixel accuracy
	 *
	 * The line is drawn only if both points reside within the clipping area of
	 * the surface.
	 *
	 * Internally, the coordinates are interpolated as fixpoint values with a
	 * fractional part of 16 bits. Therefore, the integer part of the
	 * coordinate arguments must not exceed 16 bits (on 32-bit platforms where
	 * 'long' has 32 bits).
	 *
	 * This function does not call 'surface.flush_pixels()' as it is meant
	 * to draw curve segments at a fine granularity. In this case, only one
	 * call of 'flush_pixels' per curve is preferred.
	 */
	template <typename PT>
	void paint(Genode::Surface<PT> &surface,
	           Fixpoint x1, Fixpoint y1, Fixpoint x2, Fixpoint y2,
	           Genode::Color color) const
	{
		using namespace Genode;

		/*
		 * Reduce clipping area by one pixel as the antialiased line touches an
		 * area of 2x2 for each pixel.
		 */
		Rect const clip(surface.clip().p1(), surface.clip().p2() + Point(-1, -1));

		/* both points must reside within clipping area */
		if (!surface.clip().contains(Point(x1.integer(), y1.integer())) ||
		    !surface.clip().contains(Point(x2.integer(), y2.integer())))
			return;

		long const dx_f = x2.value - x1.value,
		           dy_f = y2.value - y1.value;

		long const num_steps = max(abs(dx_f) + 127, abs(dy_f) + 127) >> 8;

		if (num_steps == 0)
			return;

		/*
		 * For the interpolation of the coordinates, we use a 16 bits for the
		 * fractional part (8 bit of the 'Fixpoint' value + 8 bit additional
		 * precision.
		 */
		long const x_ascent = (dx_f << 8)/num_steps,
		           y_ascent = (dy_f << 8)/num_steps;

		PT  const pixel(color.r, color.g, color.b);
		int const alpha = color.a;

		PT *     const dst   = surface.addr();
		unsigned const dst_w = surface.size().w();

		long x = x1.value << 8, y = y1.value << 8;

		for (long i = 0; i < num_steps; i++) {

			_transfer_pixel(dst, dst_w, pixel, alpha,
			                Fixpoint::from_raw(x >> 8),
			                Fixpoint::from_raw(y >> 8));

			x += x_ascent;
			y += y_ascent;
		}
	}

	template <typename PT>
	void paint(Genode::Surface<PT> &surface, Point p1, Point p2,
	           Genode::Color color) const
	{
		paint(surface,
		      Fixpoint::from_int(p1.x()), Fixpoint::from_int(p1.y()),
		      Fixpoint::from_int(p2.x()), Fixpoint::from_int(p2.y()),
		      color);

		surface.flush_pixels(Rect(p1, p2));
	}
};

#endif /* _INCLUDE__POLYGON_GFX__LINE_PAINTER_H_ */
