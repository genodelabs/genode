/*
 * \brief  Function for drawing the glyphs of terminal characters
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRAW_GLYPH_H_
#define _DRAW_GLYPH_H_

/* Genode includes */
#include <os/pixel_rgb565.h>

template <typename PT>
inline void draw_glyph(Genode::Color        fg_color,
                       Genode::Color        bg_color,
                       const unsigned char *glyph_base,
                       unsigned             glyph_width,
                       unsigned             glyph_img_width,
                       unsigned             glyph_img_height,
                       unsigned             cell_width,
                       PT                  *fb_base,
                       unsigned             fb_width)
{
	PT fg_pixel(fg_color.r, fg_color.g, fg_color.b);
	PT bg_pixel(bg_color.r, bg_color.g, bg_color.b);

	unsigned const horizontal_gap = cell_width
	                              - Genode::min(glyph_width, cell_width);

	unsigned const  left_gap = horizontal_gap / 2;
	unsigned const right_gap = horizontal_gap - left_gap;

	/*
	 * Clear gaps to the left and right of the character if the character's
	 * with is smaller than the cell width.
	 */
	if (horizontal_gap) {

		PT *line = fb_base;
		for (unsigned y = 0 ; y < glyph_img_height; y++, line += fb_width) {

			for (unsigned x = 0; x < left_gap; x++)
				line[x] = bg_pixel;

			for (unsigned x = cell_width - right_gap; x < cell_width; x++)
				line[x] = bg_pixel;
		}
	}

	/* center glyph horizontally within its cell */
	fb_base += left_gap;

	for (unsigned y = 0 ; y < glyph_img_height; y++) {
		for (unsigned x = 0; x < glyph_width; x++)
			fb_base[x] = PT::mix(bg_pixel, fg_pixel, glyph_base[x]);

		fb_base    += fb_width;
		glyph_base += glyph_img_width;
	}
}

#endif /* _DRAW_GLYPH_H_ */
