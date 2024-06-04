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
	using Color   = Genode::Color;
	using uint8_t = Genode::uint8_t;

	if (s == 0)
		return Color::clamped_rgb(v, v, v);

	uint8_t const region    = (uint8_t)(h / 43);
	uint8_t const remainder = (uint8_t)((h - (region*43)) * 6);
	uint8_t const p = (uint8_t)((v*(255 - s)) >> 8),
	              q = (uint8_t)((v*(255 - ((s*remainder) >> 8))) >> 8),
	              t = (uint8_t)((v*(255 - ((s*(255 - remainder)) >> 8))) >> 8);

	switch (region) {
	case 0:  return Color::clamped_rgb(v, t, p);
	case 1:  return Color::clamped_rgb(q, v, p);
	case 2:  return Color::clamped_rgb(p, v, t);
	case 3:  return Color::clamped_rgb(p, q, v);
	case 4:  return Color::clamped_rgb(t, p, v);
	}

	return Color::clamped_rgb(v, p, q);
}

#endif /* _INCLUDE__GEMS__COLOR_HSV_H_ */
