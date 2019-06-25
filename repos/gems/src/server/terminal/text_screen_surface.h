/*
 * \brief  Terminal graphics backend for textual screen
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
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
#include "color_palette.h"
#include "framebuffer.h"

namespace Terminal { template <typename> class Text_screen_surface; }


template <typename PT>
class Terminal::Text_screen_surface
{
	public:

		class Invalid_framebuffer_size : Genode::Exception { };

		typedef Text_painter::Font             Font;
		typedef Glyph_painter::Fixpoint_number Fixpoint_number;

		struct Geometry
		{
			Area            fb_size;
			Fixpoint_number char_width;
			unsigned        char_height;
			unsigned        columns;
			unsigned        lines;

			class Invalid : Genode::Exception { };

			Geometry(Font const &font, Framebuffer const &framebuffer)
			:
				fb_size(framebuffer.w(), framebuffer.h()),
				char_width(font.string_width(Utf8_ptr("M"))),
				char_height(font.height()),
				columns((framebuffer.w() << 8)/char_width.value),
				lines(framebuffer.h()/char_height)
			{
				if (columns*lines == 0)
					throw Invalid();
			}

			Rect fb_rect()   const { return Rect(Point(0, 0), fb_size); }
			Rect used_rect() const { return Rect(start(), used_pixels()); }
			Area size()      const { return Area(columns, lines); }

			/**
			 * Return pixel area covered by the character grid
			 */
			Area used_pixels() const
			{
				return Area((columns*char_width.value)>>8, lines*char_height);
			}

			/**
			 * Return excess area in pixels
			 */
			Area unused_pixels() const
			{
				return Area(fb_size.w() - used_pixels().w(),
				            fb_size.h() - used_pixels().h());
			}

			/**
			 * Return start position of character grid
			 */
			Point start() const { return Point(1, 1); }

			bool valid() const { return columns*lines > 0; }

			/**
			 * Return character position at given pixel coordinates
			 */
			Position position(Point p) const
			{
				if (char_width.value == 0 || char_height == 0)
					return Position { };

				return Position((p.x() << 8) / char_width.value, p.y() / char_height);
			}
		};

		/**
		 * Snapshot of text-screen content
		 */
		class Snapshot
		{
			private:

				friend class Text_screen_surface;

				Cell_array<Char_cell> _cell_array;

			public:

				static size_t bytes_needed(Text_screen_surface const &surface)
				{
					return Cell_array<Char_cell>::bytes_needed(surface.size().w(),
					                                           surface.size().h());
				}

				Snapshot(Allocator &alloc, Text_screen_surface const &from)
				:
					_cell_array(from._cell_array.num_cols(),
					            from._cell_array.num_lines(), alloc)
				{
					_cell_array.import_from(from._cell_array);
				}
		};

	private:

		Font          const &_font;
		Color_palette const &_palette;
		Framebuffer         &_framebuffer;
		Geometry             _geometry { _font, _framebuffer };

		Cell_array<Char_cell>            _cell_array;
		Char_cell_array_character_screen _character_screen { _cell_array };

		Decoder _decoder { _character_screen };

		struct Selection
		{
			Position start { };
			Position end   { };

			bool defined = false;

			bool selected(Position pos) const
			{
				return defined && pos.in_range(start, end);
			}

			template <typename FN>
			void for_each_line(FN const &fn) const
			{
				for (int i = min(start.y, end.y); i <= max(start.y, end.y); i++)
					fn(i);
			}

		} _selection { };

		Position _pointer { -1, -1 };

	public:

		/**
		 * Constructor
		 *
		 * \throw Geometry::Invalid
		 */
		Text_screen_surface(Allocator &alloc, Font const &font,
		                    Color_palette &palette, Framebuffer &framebuffer)
		:
			_font(font),
			_palette(palette),
			_framebuffer(framebuffer),
			_cell_array(_geometry.columns, _geometry.lines, alloc)
		{ }

		/**
		 * Update geometry
		 *
		 * Called whenever the framebuffer dimensions slightly change but
		 * without any effect on the grid size. In contrast, if the grid
		 * size changes, the entire 'Text_screen_surface' is reconstructed.
		 */
		void geometry(Geometry const &geometry)
		{
			_geometry = geometry;
			_cell_array.mark_all_lines_as_dirty(); /* trigger refresh */
		}

		Position cursor_pos() const { return _character_screen.cursor_pos(); }

		void cursor_pos(Position pos) { _character_screen.cursor_pos(pos); }

		void redraw()
		{
			PT *fb_base = _framebuffer.pixel<PT>();

			Surface<PT> surface(fb_base, _geometry.fb_size);

			unsigned const fg_alpha = 255;

			/* clear border */
			{
				Color const bg_color =
					_palette.background(Color_palette::Index{0},
					                    Color_palette::Highlighted{false});
				Rect r[4] { };
				Rect const all(Point(0, 0), _geometry.fb_size);
				_geometry.fb_rect().cut(_geometry.used_rect(), &r[0], &r[1], &r[2], &r[3]);
				for (unsigned i = 0; i < 4; i++)
					Box_painter::paint(surface, r[i], bg_color);
			}

			int const clip_top  = 0, clip_bottom = _geometry.fb_size.h(),
			          clip_left = 0, clip_right  = _geometry.fb_size.w();

			unsigned y = _geometry.start().y();
			for (unsigned line = 0; line < _cell_array.num_lines(); line++) {

				if (_cell_array.line_dirty(line)) {

					Fixpoint_number x { (int)_geometry.start().x() };
					for (unsigned column = 0; column < _cell_array.num_cols(); column++) {

						Char_cell const cell = _cell_array.get_cell(column, line);

						Codepoint codepoint = cell.codepoint();

						/* display absent codepoints as whitespace */
						bool const codepoint_valid = (codepoint.value != 0);

						bool const selected = _selection.selected(Position(column, line))
						                   && codepoint_valid;

						bool const pointer = (_pointer == Position(column, line));

						if (!codepoint_valid)
							codepoint = Codepoint{' '};

						_font.apply_glyph(codepoint, [&] (Glyph_painter::Glyph const &glyph) {

							Color_palette::Highlighted const highlighted { cell.highlight() };

							Color_palette::Index fg_idx { cell.colidx_fg() };
							Color_palette::Index bg_idx { cell.colidx_bg() };

							/* swap color index for inverse cells */
							if (cell.inverse()) {
								Color_palette::Index tmp { fg_idx };
								fg_idx = bg_idx;
								bg_idx = tmp;
							}

							Color fg_color = _palette.foreground(fg_idx, highlighted);
							Color bg_color = _palette.background(bg_idx, highlighted);

							if (selected) {
								bg_color = Color(180, 180, 180);
								fg_color = Color( 50, 50,   50);
							}

							if (pointer) {
								bg_color = Color(220, 220, 220);
								fg_color = Color( 50, 50,   50);
							}

							if (cell.has_cursor()) {
								fg_color = Color( 63,  63,  63);
								bg_color = Color(255, 255, 255);
							}

							PT const pixel(fg_color.r, fg_color.g, fg_color.b);

							Fixpoint_number next_x = x;
							next_x.value += _geometry.char_width.value;

							Box_painter::paint(surface,
							                   Rect(Point(x.decimal(), y),
							                        Point(next_x.decimal() - 1,
							                              y + _geometry.char_height - 1)),
							                   bg_color);

							/* horizontally align glyph within cell */
							x.value += (_geometry.char_width.value - (int)((glyph.width - 1)<<8)) >> 1;

							Glyph_painter::paint(Glyph_painter::Position(x, (int)y),
							                     glyph, fb_base, _geometry.fb_size.w(),
							                     clip_top, clip_bottom, clip_left, clip_right,
							                     pixel, fg_alpha);
							x = next_x;
						});
					}
				}
				y += _geometry.char_height;
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
			if (num_dirty_lines > 0) {
				int      const y = _geometry.start().y()
				                 + first_dirty_line*_geometry.char_height;
				unsigned const h = num_dirty_lines*_geometry.char_height
				                 + _geometry.unused_pixels().h();
				_framebuffer.refresh(Rect(Point(0, y),
				                          Area(_geometry.fb_size.w(), h)));
			}
		}

		void apply_character(Character c)
		{
			clear_selection();

			/* submit character to sequence decoder */
			_decoder.insert(c);
		}

		void import(Snapshot const &snapshot)
		{
			_cell_array.import_from(snapshot._cell_array);
		}

		/**
		 * Return size in colums/rows
		 */
		Area size() const { return _geometry.size(); }

		/**
		 * Set pointer position in pixels (to show the cursor)
		 */
		void pointer(Point pointer)
		{
			auto position_valid = [&] (Position pos) {
				return pos.y >= 0 && pos.y < (int)_geometry.lines; };

			/* update old position */
			if (position_valid(_pointer))
				_cell_array.mark_line_as_dirty(_pointer.y);

			_pointer = _geometry.position(pointer);

			/* update new position */
			if (position_valid(_pointer))
				_cell_array.mark_line_as_dirty(_pointer.y);
		}

		/**
		 * Set anchor point of selection
		 *
		 * \param pointer  pointer position in pixels
		 */
		void start_selection(Point pointer)
		{
			if (_selection.defined)
				clear_selection();

			_selection.start = _geometry.position(pointer);

			define_selection(pointer);
		}

		/**
		 * Set end position of current selection
		 *
		 * \param pointer  pointer position in pixels
		 */
		void define_selection(Point pointer)
		{
			_selection.for_each_line([&] (int line) {
				_cell_array.mark_line_as_dirty(line); });

			_selection.end = _geometry.position(pointer);
			_selection.defined = true;

			_selection.for_each_line([&] (int line) {
				_cell_array.mark_line_as_dirty(line); });
		}

		void clear_selection()
		{
			if (!_selection.defined)
				return;

			_selection.for_each_line([&] (int line) {
				_cell_array.mark_line_as_dirty(line); });

			_selection.defined = false;
		}

		template <typename FN>
		void for_each_selected_character(FN const &fn) const
		{
			for (unsigned row = 0; row < _geometry.lines; row++) {
				bool skip_remaining_chars_on_row = false;

				for (unsigned column = 0; column < _geometry.columns; column++) {

					if (skip_remaining_chars_on_row)
						continue;

					if (!_selection.selected(Position(column, row)))
						continue;

					Codepoint const c { _cell_array.get_cell(column, row).value };

					if (c.value == 0) {

						auto remaining_line_empty = [&] ()
						{
							for (unsigned i = column + 1; i < _geometry.columns; i++)
								if (_cell_array.get_cell(i, row).value)
									return false;

							return true;
						};

						/* generate one line break at the end of a selected line */
						if (remaining_line_empty()) {
							fn(Codepoint{'\n'});
							skip_remaining_chars_on_row = true;

						} else {
							fn(Codepoint{' '});
						}
					} else {
						fn(c);
					}
				}
			}
		}
};

#endif /* _TEXT_SCREEN_SURFACE_H_ */
