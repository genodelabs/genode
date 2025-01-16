/*
 * \brief  Blit API
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT_H_
#define _INCLUDE__BLIT_H_

#include <blit/types.h>
#include <blit/internal/slow.h>

namespace Blit {

	/**
	 * Back-to-front copy
	 *
	 * Copy a rectangular part of a texture to a surface while optionally
	 * applying rotation and flipping. The width and height of the texture
	 * must be divisible by 8. Surface and texture must perfectly line up.
	 * E.g., when rotating by 90 degrees, the texture width must equal the
	 * surface height and vice versa. The clipping area of the surface is
	 * ignored.
	 *
	 * The combination of rotate and flip arguments works as follows:
	 *
	 *                  normal         flipped
	 *
	 * rotated 0      0  1  2  3       3  2  1  0
	 *                4  5  6  7       7  6  5  4
	 *                8  9 10 11      11 10  9  8
	 *
	 * rotated 90       8  4  0          0  4  8
	 *                  9  5  1          1  5  9
	 *                 10  6  2          2  6 10
	 *                 11  7  3          3  7 11
	 *
	 * rotated 180   11 10  9  8       8  9 10 11
	 *                7  6  5  4       4  5  6  7
	 *                3  2  1  0       0  1  2  3
	 *
	 * rotated 270      3  7 11         11  7  3
	 *                  2  6 10         10  6  2
	 *                  1  5  9          9  5  1
	 *                  0  4  8          8  4  0
	 */
	static inline void back2front(Surface<Pixel_rgb888>       &surface,
	                              Texture<Pixel_rgb888> const &texture,
	                              Rect rect, Rotate rotate, Flip flip)
	{
		_b2f<Slow>(surface, texture, rect, rotate, flip);
	}
}

#endif /* _INCLUDE__BLIT_H_ */
