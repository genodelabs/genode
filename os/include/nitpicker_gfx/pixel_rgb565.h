/*
 * \brief   Template specializations for the RGB565 pixel format
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__PIXEL_RGB565_H_
#define _INCLUDE__NITPICKER_GFX__PIXEL_RGB565_H_

#include "pixel_rgb.h"

typedef Pixel_rgb<unsigned short, 0xf800, 8, 0x07e0, 3, 0x001f, -3> Pixel_rgb565;


template <>
inline Pixel_rgb565 Pixel_rgb565::avr(Pixel_rgb565 p1, Pixel_rgb565 p2)
{
	Pixel_rgb565 res;
	res.pixel = ((p1.pixel&0xf7df)>>1) + ((p2.pixel&0xf7df)>>1);
	return res;
}


template <>
inline Pixel_rgb565 Pixel_rgb565::blend(Pixel_rgb565 src, int alpha)
{
	Pixel_rgb565 res;
	res.pixel = ((((alpha >> 3) * (src.pixel & 0xf81f)) >> 5) & 0xf81f)
	          | ((( alpha       * (src.pixel & 0x07c0)) >> 8) & 0x07c0);
	return res;
}


template <>
inline Pixel_rgb565 Pixel_rgb565::mix(Pixel_rgb565 p1, Pixel_rgb565 p2, int alpha)
{
	Pixel_rgb res;

	/*
	 * We substract the alpha from 264 instead of 255 to
	 * compensate the brightness loss caused by the rounding
	 * error of the blend function when having only 5 bits
	 * per channel.
	 */
	res.pixel = blend(p1, 264 - alpha).pixel + blend(p2, alpha).pixel;
	return res;
}

#endif /* _INCLUDE__NITPICKER_GFX__PIXEL_RGB565_H_ */
