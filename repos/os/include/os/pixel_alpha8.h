/*
 * \brief  Template specializations for the ALPHA8 pixel format
 * \author Norman Feske
 * \date   2014-09-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__PIXEL_ALPHA8_H_
#define _INCLUDE__OS__PIXEL_ALPHA8_H_

#include <os/pixel_rgba.h>

namespace Genode {

	typedef Genode::Pixel_rgba<uint8_t, Genode::Surface_base::ALPHA8,
	                           0, 0, 0, 0, 0, 0, 0xff, 0>
	        Pixel_alpha8;


	/*
	 * The second pixel parameter is ignored. It can be of any pixel type.
	 */
	template <>
	template <typename PT>
	inline Pixel_alpha8 Pixel_alpha8::mix(Pixel_alpha8 p1, PT, int alpha)
	{
		Pixel_alpha8 res;
		res.pixel = (uint8_t)(p1.pixel + (((255 - p1.pixel)*alpha) >> 8));
		return res;
	}

	template <>
	inline Pixel_alpha8 Pixel_alpha8::mix(Pixel_alpha8 p1, Pixel_alpha8 p2, int alpha)
	{
		return mix<Pixel_alpha8>(p1, p2, alpha);
	}
}

#endif /* _INCLUDE__OS__PIXEL_ALPHA8_H_ */
