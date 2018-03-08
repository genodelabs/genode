/*
 * \brief  Terminal graphics backend for textual screen
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2011-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEXT_SCREEN_SURFACE_H_
#define _TEXT_SCREEN_SURFACE_H_

/* Genode includes */
#include <os/pixel_rgb565.h>

/* terminal includes */
#include <terminal/char_cell_array_character_screen.h>

/* nitpicker graphic back end */
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/box_painter.h>

/* local includes */
#include "font_family.h"
#include "color_palette.h"
#include "framebuffer.h"

namespace Terminal { template <typename> class Text_screen_surface; }


template <typename PT>
class Terminal::Text_screen_surface
{
	private:

		Font_family   const &_font_family;
		Color_palette const &_palette;
		Framebuffer         &_framebuffer;

		/* take size of space character as character cell size */
		unsigned const _char_width  { _font_family.cell_width()  };
		unsigned const _char_height { _font_family.cell_height() };

		/* number of characters fitting on the framebuffer */
		unsigned const _columns { _framebuffer.w()/_char_width  };
		unsigned const _lines   { _framebuffer.h()/_char_height };

		Cell_array<Char_cell>            _cell_array;
		Char_cell_array_character_screen _character_screen { _cell_array };

		Decoder _decoder { _character_screen };

	public:

		Text_screen_surface(Allocator &alloc, Font_family const &font_family,
		                    Color_palette &palette, Framebuffer &framebuffer)
		:
			_font_family(font_family),
			_palette(palette),
			_framebuffer(framebuffer),
			_cell_array(_columns, _lines, alloc)
		{ }

		void redraw()
		{
			Font const &regular_font = _font_family.font(Font_face::REGULAR);

			Area const cell_size = regular_font.bounding_box();

			unsigned const glyph_height = regular_font.bounding_box().h(),
			               glyph_step_x = regular_font.bounding_box().w();

			unsigned const fb_width  = _framebuffer.w(),
			               fb_height = _framebuffer.h();

			int const clip_top  = 0, clip_bottom = fb_height,
			          clip_left = 0, clip_right  = fb_width;

			PT *fb_base = _framebuffer.pixel<PT>();

			Surface<PT> surface(fb_base, Area(_framebuffer.w(), _framebuffer.h()));

			unsigned const fg_alpha = 255;

			unsigned y = 0;
			for (unsigned line = 0; line < _cell_array.num_lines(); line++) {

				if (_cell_array.line_dirty(line)) {

					unsigned x = 0;
					for (unsigned column = 0; column < _cell_array.num_cols(); column++) {

						Char_cell     cell  = _cell_array.get_cell(column, line);
						Font const   &font  = _font_family.font(cell.font_face());
						unsigned char ascii = cell.ascii;

						if (ascii == 0)
							ascii = ' ';

						Text_painter::Codepoint const c { ascii };

						font.apply_glyph(c, [&] (Glyph_painter::Glyph const &glyph) {

							Color_palette::Highlighted const highlighted { cell.highlight() };
							Color_palette::Inverse     const inverse     { cell.inverse() };

							Color fg_color =
								_palette.foreground(Color_palette::Index{cell.colidx_fg()},
								                    highlighted, inverse);

							Color bg_color =
								_palette.background(Color_palette::Index{cell.colidx_bg()},
								                    highlighted, inverse);

							if (cell.has_cursor()) {
								fg_color = Color( 63,  63,  63);
								bg_color = Color(255, 255, 255);
							}

							PT const pixel(fg_color.r, fg_color.g, fg_color.b);

							Box_painter::paint(surface, Rect(Point(x, y), cell_size), bg_color);

							Glyph_painter::paint(Glyph_painter::Position((int)x, (int)y),
							                     glyph, fb_base, fb_width,
							                     clip_top, clip_bottom, clip_left, clip_right,
							                     pixel, fg_alpha);
							x += glyph_step_x;
						});
					}
				}
				y += glyph_height;
			}

			int first_dirty_line =  10000,
			    last_dirty_line  = -10000;

			for (int line = 0; line < (int)_cell_array.num_lines(); line++) {
				if (!_cell_array.line_dirty(line)) continue;

				first_dirty_line = min(line, first_dirty_line);
				last_dirty_line  = max(line, last_dirty_line);

				_cell_array.mark_line_as_clean(line);
			}

			int const num_dirty_lines = last_dirty_line - first_dirty_line + 1;
			if (num_dirty_lines > 0)
				_framebuffer.refresh(Rect(Point(0, first_dirty_line*_char_height),
				                          Area(fb_width,
				                               num_dirty_lines*_char_height)));
		}

		void apply_character(Character c)
		{
			/* submit character to sequence decoder */
			_decoder.insert(c.c);
		}

		Session::Size size() const { return Session::Size(_columns, _lines); }
};

#endif /* _TEXT_SCREEN_SURFACE_H_ */
