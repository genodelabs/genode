/*
 * \brief   Color representation
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_GFX__COLOR_H_
#define _INCLUDE__NITPICKER_GFX__COLOR_H_

struct Color
{
	int r, g, b;

	/**
	 * Constructors
	 */
	Color(int red, int green, int blue): r(red), g(green), b(blue) { }
	Color(): r(0), g(0), b(0) { }
};

/*
 * Symbolic names for some important colors
 */
static const Color BLACK(0, 0, 0);
static const Color WHITE(255, 255, 255);
static const Color FRAME_COLOR(255, 200, 127);
static const Color KILL_COLOR(255, 0, 0);

#endif /* _INCLUDE__NITPICKER_GFX__COLOR_H_ */
