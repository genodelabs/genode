/*
 * \brief   Color-conversion between HSV and RGB color spaces
 * \date    2015-07-01
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__COLOR_HSV_H_
#define _INCLUDE__GEMS__COLOR_HSV_H_

#include <util/color.h>

/**
 * Create color from specified hue, saturation, and value
 */
static inline Genode::Color color_from_hsv(unsigned h, unsigned s, unsigned v)
{
	typedef Genode::Color Color;

	if (s == 0)
		return Color(v, v, v);

	unsigned char const region = h / 43;
	unsigned char const remainder = (h - (region*43)) * 6;
	unsigned char const p = (v*(255 - s)) >> 8,
	                    q = (v*(255 - ((s*remainder) >> 8))) >> 8,
	                    t = (v*(255 - ((s*(255 - remainder)) >> 8))) >> 8;

	switch (region) {
	case 0:  return Color(v, t, p);
	case 1:  return Color(q, v, p);
	case 2:  return Color(p, v, t);
	case 3:  return Color(p, q, v);
	case 4:  return Color(t, p, v);
	}

	return Color(v, p, q);
}

#endif /* _INCLUDE__GEMS__COLOR_HSV_H_ */
