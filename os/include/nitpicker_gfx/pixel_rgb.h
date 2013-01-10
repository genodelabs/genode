/*
 * \brief  Generic pixel representation
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__PIXEL_RGB_H_
#define _INCLUDE__NITPICKER_GFX__PIXEL_RGB_H_

/*
 * \param ST  storage type of one pixel
 */
template <typename ST, int R_MASK, int R_SHIFT,
                       int G_MASK, int G_SHIFT,
                       int B_MASK, int B_SHIFT>
class Pixel_rgb
{
	private:

		/**
		 * Shift left with positive or negative shift value
		 */
		inline int _shift(int value, int shift) {
			return shift > 0 ? value << shift : value >> -shift; }

	public:

		static const int r_mask = R_MASK, r_shift = R_SHIFT;
		static const int g_mask = G_MASK, g_shift = G_SHIFT;
		static const int b_mask = B_MASK, b_shift = B_SHIFT;

		ST pixel;

		/**
		 * Constructors
		 */
		Pixel_rgb() {}

		Pixel_rgb(int red, int green, int blue):
			pixel((_shift(red,   r_shift) & r_mask)
			    | (_shift(green, g_shift) & g_mask)
			    | (_shift(blue,  b_shift) & b_mask)) { }

		/**
		 * Compute average color value of two pixels
		 */
		static inline Pixel_rgb avr(Pixel_rgb p1, Pixel_rgb p2);

		/**
		 * Multiply pixel with alpha value
		 */
		static inline Pixel_rgb blend(Pixel_rgb pixel, int alpha);

		/**
		 * Mix two pixels at the ratio specified as alpha
		 */
		static inline Pixel_rgb mix(Pixel_rgb p1, Pixel_rgb p2, int alpha);

		/**
		 * Return alpha value of pixel
		 */
		int alpha();

} __attribute__((packed));

#endif /* _INCLUDE__NITPICKER_GFX__PIXEL_RGB_H_ */
