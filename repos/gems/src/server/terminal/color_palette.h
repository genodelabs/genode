/*
 * \brief  Terminal color palette
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COLOR_PALETTE_H_
#define _COLOR_PALETTE_H_

/* Genode includes */
#include <util/color.h>

/* local includes */
#include "types.h"

namespace Terminal {
	class Color_palette;
}


class Terminal::Color_palette
{
	public:

		struct Index { unsigned value; };

		struct Highlighted { bool value; };

		struct Inverse { bool value; };

	private:

		enum { NUM_COLORS = 16U };

		Color _colors[NUM_COLORS];

	public:

		Color_palette()
		{
			_colors[0] = Color(  0,   0,   0);  /* black */
			_colors[1] = Color(255, 128, 128);  /* red */
			_colors[2] = Color(128, 255, 128);  /* green */
			_colors[3] = Color(255, 255,   0);  /* yellow */
			_colors[4] = Color(128, 128, 255);  /* blue */
			_colors[5] = Color(255,   0, 255);  /* magenta */
			_colors[6] = Color(  0, 255, 255);  /* cyan */
			_colors[7] = Color(255, 255, 255);  /* white */

			/* the upper portion of the palette contains highlight colors */
			for (unsigned i = 0; i < NUM_COLORS/2; i++) {
				Color const col = _colors[i];
				_colors[i + NUM_COLORS/2] = Color((col.r*2)/3, (col.g*2)/3, (col.b*2)/3);
			}
		}

		Color foreground(Index index, Highlighted highlighted, Inverse inverse) const
		{
			if (index.value >= NUM_COLORS/2)
				return Color(0, 0, 0);

			Color const col =
				_colors[index.value + (highlighted.value ? NUM_COLORS/2 : 0)];

			return (inverse.value) ? Color(col.r/2, col.g/2, col.b/2) : col;
		}

		Color background(Index index, Highlighted highlighted, Inverse inverse) const
		{
			Color const color =
				_colors[index.value + (highlighted.value ? NUM_COLORS/2 : 0)];

			return (inverse.value) ? Color((color.r + 255)/2,
			                               (color.g + 255)/2,
			                               (color.b + 255)/2)
			                       : color;
		}
};

#endif /* _COLOR_PALETTE_H_ */
