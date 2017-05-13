/*
 * \brief  Array of character cells
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__CELL_ARRAY_H_
#define _TERMINAL__CELL_ARRAY_H_

/* Genode includes */
#include <base/allocator.h>


/**
 * \param CELL  type of a single character cell that contains the information
 *              about the glyph and its attributes
 *
 * The 'CELL' type must have a default constructor and has to provide the
 * methods 'set_cursor()' and 'clear_cursor'.
 */
template <typename CELL>
class Cell_array
{
	private:

		unsigned           _num_cols;
		unsigned           _num_lines;
		Genode::Allocator *_alloc;
		CELL             **_array;
		bool              *_line_dirty;

		typedef CELL *Char_cell_line;

		void _clear_line(Char_cell_line line)
		{
			for (unsigned col = 0; col < _num_cols; col++)
				*line++ = CELL();
		}

		void _mark_lines_as_dirty(int start, int end)
		{
			for (int line = start; line <= end; line++)
				_line_dirty[line] = true;
		}

		void _scroll_vertically(int start, int end, bool up)
		{
			/* rotate lines of the scroll region */
			Char_cell_line yanked_line = _array[up ? start : end];

			if (up) {
				for (int line = start; line <= end - 1; line++)
					_array[line] = _array[line + 1];
			} else {
				for (int line = end; line >= start + 1; line--)
					_array[line] = _array[line - 1];
			}

			_clear_line(yanked_line);

			_array[up ? end: start] = yanked_line;

			_mark_lines_as_dirty(start, end);
		}

	public:

		Cell_array(unsigned num_cols, unsigned num_lines,
		           Genode::Allocator *alloc)
		:
			_num_cols(num_cols),
			_num_lines(num_lines),
			_alloc(alloc)
		{
			_array = new (alloc) Char_cell_line[num_lines];

			_line_dirty = new (alloc) bool[num_lines];
			for (unsigned i = 0; i < num_lines; i++)
				_line_dirty[i] = false;

			for (unsigned i = 0; i < num_lines; i++)
				_array[i] = new (alloc) CELL[num_cols];
		}

		~Cell_array()
		{
			for (unsigned i = 0; i < _num_lines; i++)
				Genode::destroy(_alloc, _array[i]);

			Genode::destroy(_alloc, _line_dirty);
			Genode::destroy(_alloc, _array);
		}

		void set_cell(int column, int line, CELL cell)
		{
			_array[line][column] = cell;
			_line_dirty[line] = true;
		}

		CELL get_cell(int column, int line)
		{
			return _array[line][column];
		}

		bool line_dirty(int line) { return _line_dirty[line]; }

		void mark_line_as_clean(int line)
		{
			_line_dirty[line] = false;
		}

		void mark_line_as_dirty(int line)
		{
			_line_dirty[line] = true;
		}

		void scroll_up(int region_start, int region_end)
		{
			_scroll_vertically(region_start, region_end, true);
		}

		void scroll_down(int region_start, int region_end)
		{
			_scroll_vertically(region_start, region_end, false);
		}

		void clear(int region_start, int region_end)
		{
			for (int line = region_start; line <= region_end; line++)
				_clear_line(_array[line]);

			_mark_lines_as_dirty(region_start, region_end);
		}

		void cursor(Terminal::Position pos, bool enable, bool mark_dirty = false)
		{
			if (((unsigned)pos.x >= _num_cols) ||
			    ((unsigned)pos.y >= _num_lines))
				return;

			CELL &cell = _array[pos.y][pos.x];

			if (enable)
				cell.set_cursor();
			else
				cell.clear_cursor();

			if (mark_dirty)
				_line_dirty[pos.y] = true;
		}

		unsigned num_cols()  { return _num_cols; }
		unsigned num_lines() { return _num_lines; }
};

#endif /* _TERMINAL__CELL_ARRAY_H_ */
