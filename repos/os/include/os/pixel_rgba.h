/*
 * \brief  Generic pixel representation
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__PIXEL_RGBA_H_
#define _INCLUDE__OS__PIXEL_RGBA_H_

#include <os/surface.h>

namespace Genode {
	template <typename ST, Genode::Surface_base::Pixel_format FORMAT,
	          int R_MASK, int R_SHIFT,
	          int G_MASK, int G_SHIFT,
	          int B_MASK, int B_SHIFT,
	          int A_MASK, int A_SHIFT>
	class Pixel_rgba;
}


/*
 * \param ST      storage type of one pixel
 * \param FORMAT  pixel format
 */
template <typename ST, Genode::Surface_base::Pixel_format FORMAT,
          int R_MASK, int R_SHIFT,
          int G_MASK, int G_SHIFT,
          int B_MASK, int B_SHIFT,
          int A_MASK, int A_SHIFT>
class Genode::Pixel_rgba
{
	private:

		/**
		 * Shift left with positive or negative shift value
		 */
		static inline int _shift(int value, int shift) {
			return shift > 0 ? value << shift : value >> -shift; }

	public:

		static const int r_mask = R_MASK, r_shift = R_SHIFT;
		static const int g_mask = G_MASK, g_shift = G_SHIFT;
		static const int b_mask = B_MASK, b_shift = B_SHIFT;
		static const int a_mask = A_MASK, a_shift = A_SHIFT;

		ST pixel = 0;

		/**
		 * Constructors
		 */
		Pixel_rgba() {}

		Pixel_rgba(int red, int green, int blue, int alpha = 255) :
			pixel((_shift(red,   r_shift) & r_mask)
			    | (_shift(green, g_shift) & g_mask)
			    | (_shift(blue,  b_shift) & b_mask)
			    | (_shift(alpha, a_shift) & a_mask)) { }

		static Surface_base::Pixel_format format() { return FORMAT; }

		/**
		 * Assign new rgba values
		 */
		void rgba(int red, int green, int blue, int alpha = 255)
		{
			pixel = (_shift(red,   r_shift) & r_mask)
			      | (_shift(green, g_shift) & g_mask)
			      | (_shift(blue,  b_shift) & b_mask)
			      | (_shift(alpha, a_shift) & a_mask);
		}

		inline int r() const { return _shift(pixel & r_mask, -r_shift); }
		inline int g() const { return _shift(pixel & g_mask, -g_shift); }
		inline int b() const { return _shift(pixel & b_mask, -b_shift); }
		inline int a() const { return _shift(pixel & a_mask, -a_shift); }

		/**
		 * Compute average color value of two pixels
		 */
		static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2);

		/**
		 * Multiply pixel with alpha value
		 */
		static inline Pixel_rgba blend(Pixel_rgba pixel, int alpha);

		/**
		 * Mix two pixels at the ratio specified as alpha
		 */
		static inline Pixel_rgba mix(Pixel_rgba p1, Pixel_rgba p2, int alpha);

		/**
		 * Mix two pixels where p2 may be of a different pixel format
		 *
		 * This is useful for drawing operations that apply the same texture
		 * to a pixel surface as well as an alpha surface. When drawing on the
		 * alpha surface, 'p1' will be of type 'Pixel_alpha8' whereas 'p2'
		 * corresponds to the pixel type of the texture.
		 */
		template <typename PT>
		static inline Pixel_rgba mix(Pixel_rgba p1, PT p2, int alpha);

		/**
		 * Compute average color value of four pixels
		 */
		static inline Pixel_rgba avr(Pixel_rgba p1, Pixel_rgba p2,
		                             Pixel_rgba p3, Pixel_rgba p4) {
			return avr(avr(p1, p2), avr(p3, p4)); }

		/**
		 * Copy pixel with alpha
		 *
		 * \param src    source color value (e.g., obtained from a texture)
		 * \param src_a  alpha value corresponding to the 'src' pixel
		 * \param alpha  alpha value
		 * \param dst    destination pixel
		 */
		template <typename TPT, typename PT>
		static void transfer(TPT const &src, int src_a, int alpha, PT &dst)
		{
			if (src_a) {
				int a = (src_a * alpha)>>8;
				if (a) dst = PT::mix(dst, src, a);
			}
		}

		/**
		 * Return alpha value of pixel
		 */
		int alpha();

} __attribute__((packed));

#endif /* _INCLUDE__OS__PIXEL_RGBA_H_ */
