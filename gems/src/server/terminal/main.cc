/*
 * \brief  Terminal service
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/heap.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <os/ring_buffer.h>
#include <os/attached_ram_dataspace.h>
#include <input/keycodes.h>
#include <input/event.h>
#include <os/config.h>

#include <terminal/decoder.h>
#include <terminal/types.h>

#include <nitpicker_gfx/font.h>
#include <nitpicker_gfx/color.h>
#include <nitpicker_gfx/pixel_rgb565.h>

#include <terminal_session/terminal_session.h>

static bool const verbose = false;

enum { READ_BUFFER_SIZE = 4096 };

class Read_buffer : public Ring_buffer<unsigned char, READ_BUFFER_SIZE>
{
	private:

		Genode::Signal_context_capability _sigh_cap;

	public:

		/**
		 * Register signal handler for read-avail signals
		 */
		void sigh(Genode::Signal_context_capability cap)
		{
			_sigh_cap = cap;
		}

		/**
		 * Add element into read buffer and emit signal
		 */
		void add(unsigned char c)
		{
			Ring_buffer<unsigned char, READ_BUFFER_SIZE>::add(c);

			if (_sigh_cap.valid())
				Genode::Signal_transmitter(_sigh_cap).submit();
		}

		void add(char const *str)
		{
			while (*str)
				add(*str++);
		}
};


inline Pixel_rgb565 blend(Pixel_rgb565 src, int alpha)
{
	Pixel_rgb565 res;
	res.pixel = ((((alpha >> 3) * (src.pixel & 0xf81f)) >> 5) & 0xf81f)
	          | ((( alpha       * (src.pixel & 0x07c0)) >> 8) & 0x07c0);
	return res;
}


inline Pixel_rgb565 mix(Pixel_rgb565 p1, Pixel_rgb565 p2, int alpha)
{
	Pixel_rgb565 res;

	/*
	 * We substract the alpha from 264 instead of 255 to
	 * compensate the brightness loss caused by the rounding
	 * error of the blend function when having only 5 bits
	 * per channel.
	 */
	res.pixel = blend(p1, 264 - alpha).pixel + blend(p2, alpha).pixel;
	return res;
}

/*
 * Font initialization
 */
extern char _binary_mono_tff_start;

static Font mono_font   (&_binary_mono_tff_start);

static Font *fonts[4] = { &mono_font, &mono_font,
                          &mono_font, &mono_font };

enum Font_face { FONT_REGULAR, FONT_ITALIC, FONT_BOLD, FONT_BOLD_ITALIC };


static Color color_palette[2*8];


struct Char_cell
{
	unsigned char attr;
	unsigned char ascii;

	enum { ATTR_COLIDX_MASK = 0x07,
	       ATTR_CURSOR      = 0x10,
	       ATTR_INVERSE     = 0x20,
	       ATTR_HIGHLIGHT   = 0x40 };

	Char_cell() : attr(0), ascii(0) { }

	Char_cell(unsigned char c, Font_face f, int colidx, bool inv, bool highlight)
	:
		attr(f | (colidx & ATTR_COLIDX_MASK)
		       | (inv ? ATTR_INVERSE : 0)
		       | (highlight ? ATTR_HIGHLIGHT : 0)),
		ascii(c)
	{ }

	Font_face font_face() const { return (Font_face)(attr & 0x3); }

	int  colidx()  const { return attr & ATTR_COLIDX_MASK; }
	bool inverse() const { return attr & ATTR_INVERSE;     }
	bool highlight()  const { return attr & ATTR_HIGHLIGHT;   }

	Color fg_color() const
	{
		Color col = color_palette[colidx() + (highlight() ? 8 : 0)];

		if (inverse())
			col = Color(col.r/2, col.g/2, col.b/2);

		return col;
	}

	Color bg_color() const
	{
		Color col = color_palette[colidx() + (highlight() ? 8 : 0)];

		if (inverse())
			return Color((col.r + 255)/2, (col.g + 255)/2, (col.b + 255)/2);
		else
			return Color(0, 0, 0);
	}

	void set_cursor()   { attr |=  ATTR_CURSOR; }
	void clear_cursor() { attr &= ~ATTR_CURSOR; }

	bool has_cursor() const { return attr & ATTR_CURSOR; }
};


class Char_cell_array
{
	private:

		unsigned           _num_cols;
		unsigned           _num_lines;
		Genode::Allocator *_alloc;
		Char_cell        **_array;
		bool              *_line_dirty;

		typedef Char_cell *Char_cell_line;

		void _clear_line(Char_cell_line line)
		{
			for (unsigned col = 0; col < _num_cols; col++)
				*line++ = Char_cell();
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

		Char_cell_array(unsigned num_cols, unsigned num_lines,
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
				_array[i] = new (alloc) Char_cell[num_cols];
		}

		void set_cell(int column, int line, Char_cell cell)
		{
			_array[line][column] = cell;
			_line_dirty[line] = true;
		}

		Char_cell get_cell(int column, int line)
		{
			return _array[line][column];
		}

		bool line_dirty(int line) { return _line_dirty[line]; }

		void mark_line_as_clean(int line)
		{
			_line_dirty[line] = false;
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
			Char_cell &cell = _array[pos.y][pos.x];

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


class Char_cell_array_character_screen : public Terminal::Character_screen
{
	private:

		enum Cursor_visibility { CURSOR_INVISIBLE,
		                         CURSOR_VISIBLE,
		                         CURSOR_VERY_VISIBLE };

		Char_cell_array    &_char_cell_array;
		Terminal::Boundary  _boundary;
		Terminal::Position  _cursor_pos;
		int                 _color_index;
		bool                _inverse;
		bool                _highlight;
		Cursor_visibility   _cursor_visibility;
		int                 _region_start;
		int                 _region_end;
		int                 _tab_size;

		enum { DEFAULT_COLOR_INDEX = 7, DEFAULT_TAB_SIZE = 8 };

		struct Cursor_guard
		{
			Char_cell_array_character_screen &cs;

			Terminal::Position old_cursor_pos;

			Cursor_guard(Char_cell_array_character_screen &cs)
			:
				cs(cs), old_cursor_pos(cs._cursor_pos)
			{
				/* temporarily remove cursor */
				cs._char_cell_array.cursor(old_cursor_pos, false);
			}

			~Cursor_guard()
			{
				/* restore original cursor */
				cs._char_cell_array.cursor(old_cursor_pos, true);

				/* if cursor position changed, move cursor */
				Terminal::Position &new_cursor_pos = cs._cursor_pos;
				if (old_cursor_pos != new_cursor_pos) {

					cs._char_cell_array.cursor(old_cursor_pos, false, true);
					cs._char_cell_array.cursor(new_cursor_pos, true,  true);
				}
			}
		};

	public:

		Char_cell_array_character_screen(Char_cell_array &char_cell_array)
		:
			_char_cell_array(char_cell_array),
			_boundary(_char_cell_array.num_cols(), _char_cell_array.num_lines()),
			_color_index(DEFAULT_COLOR_INDEX),
			_inverse(false),
			_highlight(false),
			_cursor_visibility(CURSOR_VISIBLE),
			_region_start(0),
			_region_end(_boundary.height - 1),
			_tab_size(DEFAULT_TAB_SIZE)
		{ }


		void _line_feed()
		{
			Cursor_guard guard(*this);

			_cursor_pos.y++;

			if (_cursor_pos.y > _region_end) {
				_char_cell_array.scroll_up(_region_start, _region_end);
				_cursor_pos.y = _region_end;
			}
		}

		void _carriage_return()
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = 0;
		}


		/********************************
		 ** Character_screen interface **
		 ********************************/

		Terminal::Position cursor_pos() const { return _cursor_pos; }

		void output(Terminal::Character c)
		{
			if (c.ascii() > 0x10) {
				Cursor_guard guard(*this);
				_char_cell_array.set_cell(_cursor_pos.x, _cursor_pos.y,
				                          Char_cell(c.ascii(), FONT_REGULAR,
				                          _color_index, _inverse, _highlight));
				_cursor_pos.x++;
			}

			switch (c.ascii()) {

			case '\r': /* 13 */
				_carriage_return();
				break;

			case '\n': /* 10 */
				_line_feed();
				_carriage_return();
				break;

			case 8: /* backspace */
				{
					Cursor_guard guard(*this);

					if (_cursor_pos.x > 0)
						_cursor_pos.x--;

					break;
				}

			case 9: /* tab */
				{
					Cursor_guard guard(*this);
					_cursor_pos.x += _tab_size - (_cursor_pos.x % _tab_size);
					break;
				}

			default:
				break;
			}

			if (_cursor_pos.x >= _boundary.width) {
				_carriage_return();
				_line_feed();
			}
		}

		void civis()
		{
			_cursor_visibility = CURSOR_INVISIBLE;
		}

		void cnorm()
		{
			_cursor_visibility = CURSOR_VISIBLE;
		}

		void cvvis()
		{
			_cursor_visibility = CURSOR_VERY_VISIBLE;
		}

		void cpr()
		{
			PDBG("not implemented");
		}

		void csr(int start, int end)
		{
			/* the arguments are specified use coordinate origin (1, 1) */
			start--;
			end--;

			_region_start = Genode::max(start, 0);
			_region_end   = Genode::min(end, _boundary.height - 1);

			/* preserve invariant of region size >= 0 */
			_region_end = Genode::max(_region_end, _region_start);
		}

		void cuf(int dx)
		{
			Cursor_guard guard(*this);

			_cursor_pos.x += dx;
			_cursor_pos.x = Genode::min(_boundary.width - 1, _cursor_pos.x);
		}

		void cup(int y, int x)
		{
			Cursor_guard guard(*this);

			/* top-left cursor position is reported as (1, 1) */
			x--;
			y--;

			using namespace Genode;
			x = max(0, min(x, _boundary.width  - 1));
			y = max(0, min(y, _boundary.height - 1));

			_cursor_pos = Terminal::Position(x, y);
		}

		void cuu1()
		{
			PWRN("not implemented");
		}

		void dch(int)
		{
			PDBG("not implemented");
		}

		void dl(int num_lines)
		{
			/* delete number of lines */
			for (int i = 0; i < num_lines; i++)
				_char_cell_array.scroll_up(_cursor_pos.y, _region_end);
		}

		void ech(int v)
		{
			Cursor_guard guard(*this);

			for (int i = 0; i < v; i++, _cursor_pos.x++)
				_char_cell_array.set_cell(_cursor_pos.x, _cursor_pos.y,
				                          Char_cell(' ', FONT_REGULAR,
				                          _color_index, _inverse, _highlight));
		}

		void ed()
		{
			/* clear to end of screen */
			_char_cell_array.clear(_cursor_pos.y, _boundary.height - 1);
		}

		void el()
		{
			/* clear to end of line */
			for (int x = _cursor_pos.x; x < _boundary.width; x++)
				_char_cell_array.set_cell(x, _cursor_pos.y, Char_cell());
		}

		void el1()
		{
			PDBG("not implemented");
		}

		void home()
		{
			Cursor_guard guard(*this);

			_cursor_pos = Terminal::Position(0, 0);
		}

		void hpa(int x)
		{
			Cursor_guard guard(*this);

			_cursor_pos.x = x;
			_cursor_pos.x = Genode::min(_boundary.width - 1, _cursor_pos.x);
		}

		void hts()
		{
			PDBG("not implemented");
		}

		void ich(int)
		{
			PDBG("not implemented");
		}

		void il(int value)
		{
			Cursor_guard guard(*this);

			if (_cursor_pos.y > _region_end)
				return;

			_char_cell_array.cursor(_cursor_pos, false);

			for (int i = 0; i < value; i++)
				_char_cell_array.scroll_down(_cursor_pos.y, _region_end);

			_char_cell_array.cursor(_cursor_pos, true);
		}

		void oc()
		{
			PDBG("not implemented");
		}

		void op()
		{
			_color_index = DEFAULT_COLOR_INDEX;
		}

		void rc()
		{
			PDBG("not implemented");
		}

		void ri()
		{
			PDBG("not implemented");
		}

		void ris()
		{
			PDBG("not implemented");
		}

		void rmam()
		{
			PDBG("not implemented");
		}

		void rmir()
		{
			PDBG("not implemented");
		}

		void setab(int value)
		{
			_inverse = (value != 0);
		}

		void setaf(int value)
		{
			_color_index = value;
		}

		void sgr(int value)
		{
			_highlight = (value & 0x1) != 0;
			_inverse   = (value & 0x2) != 0;

			/* sgr 0 is the command to reset all attributes, including color */
			if (value == 0)
				_color_index = DEFAULT_COLOR_INDEX;
		}

		void sgr0()
		{
			sgr(0);
		}

		void sc()
		{
			PDBG("not implemented");
		}

		void smam()
		{
			PDBG("not implemented");
		}

		void smir()
		{
			PDBG("not implemented");
		}

		void tbc()
		{
			PDBG("not implemented");
		}

		void u6(int, int)
		{
			PDBG("not implemented");
		}

		void u7()
		{
			PDBG("not implemented");
		}

		void u8()
		{
			PDBG("not implemented");
		}

		void u9()
		{
			PDBG("not implemented");
		}

		void vpa(int y)
		{
			Cursor_guard guard(*this);

			_cursor_pos.y = y;
			_cursor_pos.y = Genode::min(_boundary.height - 1, _cursor_pos.y);
		}
};


template <typename PT>
inline void draw_glyph(Color                fg_color,
                       Color                bg_color,
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
			fb_base[x] = mix(bg_pixel, fg_pixel, glyph_base[x]);

		fb_base    += fb_width;
		glyph_base += glyph_img_width;
	}
}


template <typename PT>
static void convert_char_array_to_pixels(Char_cell_array *cell_array,
                                         PT              *fb_base,
                                         unsigned         fb_width,
                                         unsigned         fb_height)
{
	unsigned glyph_height = mono_font.img_h,
	         glyph_step_x = mono_font.wtab['m'];

	unsigned y = 0;
	for (unsigned line = 0; line < cell_array->num_lines(); line++) {

		if (cell_array->line_dirty(line)) {

			if (verbose)
				Genode::printf("convert line %d\n", line);

			unsigned x = 0;
			for (unsigned column = 0; column < cell_array->num_cols(); column++) {

				Char_cell      cell  = cell_array->get_cell(column, line);
				Font          *font  = fonts[cell.font_face()];
				unsigned char  ascii = cell.ascii;

				if (ascii == 0)
					ascii = ' ';

				unsigned char *glyph_base  = font->img + font->otab[ascii];

				unsigned glyph_width = mono_font.wtab[ascii];

				if (x + glyph_width >= fb_width) break;

				Color fg_color = cell.fg_color();
				Color bg_color = cell.bg_color();

				if (cell.has_cursor()) {
					fg_color = Color( 63,  63,  63);
					bg_color = Color(255, 255, 255);
				}

				draw_glyph<PT>(fg_color, bg_color,
				               glyph_base, glyph_width,
				               (unsigned)font->img_w, (unsigned)font->img_h,
				               glyph_step_x, fb_base + x, fb_width);

				x += glyph_step_x;
			}
		}
		y       += glyph_height;
		fb_base += fb_width*glyph_height;

		if (y + glyph_height > fb_height) break;
	}
}


namespace Terminal {

	struct Flush_callback : Genode::List<Flush_callback>::Element
	{
		virtual void flush() = 0;
	};


	struct Flush_callback_registry
	{
		Genode::List<Flush_callback> _list;
		Genode::Lock _lock;

		void add(Flush_callback *flush_callback)
		{
			Genode::Lock::Guard guard(_lock);
			_list.insert(flush_callback);
		}

		void remove(Flush_callback *flush_callback)
		{
			Genode::Lock::Guard guard(_lock);
			_list.remove(flush_callback);
		}

		void flush()
		{
			Genode::Lock::Guard guard(_lock);
			Flush_callback *curr = _list.first();
			for (; curr; curr = curr->next())
				curr->flush();
		}
	};


	class Session_component : public Genode::Rpc_object<Session, Session_component>,
	                          public Flush_callback
	{
		private:

			Read_buffer                   *_read_buffer;
			Framebuffer::Session          *_framebuffer;

			Flush_callback_registry       &_flush_callback_registry;

			Genode::Attached_ram_dataspace _io_buffer;

			Framebuffer::Mode              _fb_mode;
			Genode::Dataspace_capability   _fb_ds_cap;
			unsigned                       _char_width;
			unsigned                       _char_height;
			unsigned                       _columns;
			unsigned                       _lines;
			void                          *_fb_addr;

			/**
			 * Protect '_char_cell_array'
			 */
			Genode::Lock _lock;

			Char_cell_array                  _char_cell_array;
			Char_cell_array_character_screen _char_cell_array_character_screen;
			Terminal::Decoder                _decoder;

			Terminal::Position _last_cursor_pos;

			/**
			 * Initialize framebuffer-related attributes
			 */
			Genode::Dataspace_capability _init_fb()
			{
				if (_fb_mode.format() != Framebuffer::Mode::RGB565) {
					PERR("Color mode %d not supported", _fb_mode.format());
					return Genode::Dataspace_capability();
				}

				return _framebuffer->dataspace();
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(Read_buffer             *read_buffer,
			                  Framebuffer::Session    *framebuffer,
			                  Genode::size_t           io_buffer_size,
			                  Flush_callback_registry &flush_callback_registry)
			:
				_read_buffer(read_buffer), _framebuffer(framebuffer),
				_flush_callback_registry(flush_callback_registry),
				_io_buffer(Genode::env()->ram_session(), io_buffer_size),
				_fb_mode(_framebuffer->mode()),
				_fb_ds_cap(_init_fb()),

				/* take size of space character as character cell size */
				_char_width(mono_font.str_w("m")),
				_char_height(mono_font.str_h("m")),

				/* compute number of characters fitting on the framebuffer */
				_columns(_fb_mode.width()/_char_width),
				_lines(_fb_mode.height()/_char_height),

				_fb_addr(Genode::env()->rm_session()->attach(_fb_ds_cap)),
				_char_cell_array(_columns, _lines, Genode::env()->heap()),
				_char_cell_array_character_screen(_char_cell_array),
				_decoder(_char_cell_array_character_screen)
			{
				using namespace Genode;

				printf("new terminal session:\n");
				printf("  framebuffer has %dx%d pixels\n", _fb_mode.width(), _fb_mode.height());
				printf("  character size is %dx%d pixels\n", _char_width, _char_height);
				printf("  terminal size is %dx%d characters\n", _columns, _lines);

				framebuffer->refresh(0, 0, _fb_mode.width(), _fb_mode.height());

				_flush_callback_registry.add(this);
			}

			~Session_component()
			{
				_flush_callback_registry.remove(this);
			}

			void flush()
			{
				Genode::Lock::Guard guard(_lock);

				convert_char_array_to_pixels<Pixel_rgb565>(&_char_cell_array,
				                                           (Pixel_rgb565 *)_fb_addr,
				                                           _fb_mode.width(),
				                                           _fb_mode.height());

				
				int first_dirty_line =  10000,
				    last_dirty_line  = -10000;

				for (int line = 0; line < (int)_char_cell_array.num_lines(); line++) {
					if (!_char_cell_array.line_dirty(line)) continue;

					first_dirty_line = Genode::min(line, first_dirty_line);
					last_dirty_line  = Genode::max(line, last_dirty_line);

					_char_cell_array.mark_line_as_clean(line);
				}

				int num_dirty_lines = last_dirty_line - first_dirty_line + 1;

				_framebuffer->refresh(0, first_dirty_line*_char_height,
				                      _fb_mode.width(), num_dirty_lines*_char_height);
			}


			/********************************
			 ** Terminal session interface **
			 ********************************/

			Size size() { return Size(_columns, _lines); }

			bool avail() { return !_read_buffer->empty(); }

			Genode::size_t _read(Genode::size_t dst_len)
			{
				/* read data, block on first byte if needed */
				unsigned       num_bytes = 0;
				unsigned char *dst       = _io_buffer.local_addr<unsigned char>();
				Genode::size_t dst_size  = Genode::min(_io_buffer.size(), dst_len);
				do {
					dst[num_bytes++] = _read_buffer->get();;
				} while (!_read_buffer->empty() && num_bytes < dst_size);

				return num_bytes;
			}

			void _write(Genode::size_t num_bytes)
			{
				Genode::Lock::Guard guard(_lock);

				unsigned char *src = _io_buffer.local_addr<unsigned char>();

				for (unsigned i = 0; i < num_bytes; i++) {
					if (verbose)
						Genode::printf("%c (%d)\n", src[i], (int)src[i]);

					/* submit character to sequence decoder */
					_decoder.insert(src[i]);
				}
			}

			Genode::Dataspace_capability _dataspace()
			{
				return _io_buffer.cap();
			}

			void connected_sigh(Genode::Signal_context_capability sigh)
			{
				/*
				 * Immediately reflect connection-established signal to the
				 * client because the session is ready to use immediately after
				 * creation.
				 */
				Genode::Signal_transmitter(sigh).submit();
			}

			void read_avail_sigh(Genode::Signal_context_capability cap)
			{
				_read_buffer->sigh(cap);
			}

			Genode::size_t read(void *buf, Genode::size_t)  { return 0; }
			Genode::size_t write(void const *buf, Genode::size_t) { return 0; }
	};


	class Root_component : public Genode::Root_component<Session_component>
	{
		private:

			Read_buffer             *_read_buffer;
			Framebuffer::Session    *_framebuffer;
			Flush_callback_registry &_flush_callback_registry;

		protected:

			Session_component *_create_session(const char *args)
			{
				Genode::printf("create terminal session\n");

				/*
				 * XXX read I/O buffer size from args
				 */
				Genode::size_t io_buffer_size = 4096;

				Session_component *session =
					new (md_alloc()) Session_component(_read_buffer,
					                                   _framebuffer,
					                                   io_buffer_size,
					                                   _flush_callback_registry);
				return session;
			}

		public:

			/**
			 * Constructor
			 */
			Root_component(Genode::Rpc_entrypoint  *ep,
			               Genode::Allocator       *md_alloc,
			               Read_buffer             *read_buffer,
			               Framebuffer::Session    *framebuffer,
			               Flush_callback_registry &flush_callback_registry)
			:
				Genode::Root_component<Session_component>(ep, md_alloc),
				_read_buffer(read_buffer), _framebuffer(framebuffer),
				_flush_callback_registry(flush_callback_registry)
			{ }
	};
}


enum {
	BS  =   8,
	ESC =  27,
	TAB =   9,
	LF  =  10,
	UE  = 252,   /* 'ü' */
	AE  = 228,   /* 'ä' */
	OE  = 246,   /* 'ö' */
	PAR = 167,   /* '§' */
	DEG = 176,   /* '°' */
	SS  = 223,   /* 'ß' */
};


static unsigned char usenglish_keymap[128] = {
	 0 ,ESC,'1','2','3','4','5','6','7','8','9','0','-','=', BS,TAB,
	'q','w','e','r','t','y','u','i','o','p','[',']', LF, 0 ,'a','s',
	'd','f','g','h','j','k','l',';','\'','`', 0, '\\' ,'z','x','c','v',
	'b','n','m',',','.','/', 0 , 0 , 0 ,' ', 0 , 0 , 0 , 0 , 0 , 0 ,
	 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'7','8','9','-','4','5','6','+','1',
	'2','3','0',',', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	 LF, 0 ,'/', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
};


/**
 * Mapping from ASCII value to another ASCII value when shift is pressed
 *
 * The table does not contain mappings for control characters. The table
 * entry 0 corresponds to ASCII value 32.
 */
static unsigned char usenglish_shift[256 - 32] = {
	/*  32 */ ' ', 0 , 0,  0 , 0 , 0 , 0 ,'"', 0 , 0 , 0 , 0 ,'<','_','>','?',
	/*  48 */ ')','!','@','#','$','%','^','&','*','(', 0 ,':', 0 ,'+', 0 , 0 ,
	/*  64 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/*  80 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'{','|','}', 0 , 0 ,
	/*  96 */ '~','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
	/* 112 */ 'P','Q','R','S','T','U','V','W','X','Y','Z', 0 ,'\\', 0 , 0 , 0 ,
};


static unsigned char german_keymap[128] = {
	 0 ,ESC,'1','2','3','4','5','6','7','8','9','0', SS, 39, BS,TAB,
	'q','w','e','r','t','z','u','i','o','p', UE,'+', LF, 0 ,'a','s',
	'd','f','g','h','j','k','l', OE, AE,'^', 0 ,'#','y','x','c','v',
	'b','n','m',',','.','-', 0 ,'*', 0 ,' ', 0 , 0 , 0 , 0 , 0 , 0 ,
	 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'7','8','9','-','4','5','6','+','1',
	'2','3','0',',', 0 , 0 ,'<', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	 LF, 0 ,'/', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
};


/**
 * Mapping from ASCII value to another ASCII value when shift is pressed
 *
 * The table does not contain mappings for control characters. The table
 * entry 0 corresponds to ASCII value 32.
 */
static unsigned char german_shift[256 - 32] = {
	/*  32 */ ' ', 0 , 0, 39 , 0 , 0 , 0 ,'`', 0 , 0 , 0 ,'*',';','_',':', 0 ,
	/*  48 */ '=','!','"',PAR,'$','%','&','/','(',')', 0 , 0 ,'>', 0 , 0 , 0 ,
	/*  64 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/*  80 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,DEG, 0 ,
	/*  96 */  0 ,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
	/* 112 */ 'P','Q','R','S','T','U','V','W','X','Y','Z', 0 , 0 , 0 , 0 , 0 ,
	/* 128 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 144 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 160 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 176 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 192 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 208 */  0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'?',
};


/**
 * Mapping from ASCII value to another ASCII value when altgr is pressed
 */
static unsigned char german_altgr[256 - 32] = {
	/*  32 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'~', 0 , 0 , 0 , 0 ,
	/*  48 */'}', 0 ,178,179, 0 , 0 , 0 ,'{','[',']', 0 , 0 ,'|', 0 , 0 , 0 ,
	/*  64 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/*  80 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/*  96 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 112 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 128 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 144 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 160 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 176 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 192 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
	/* 208 */ 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,'\\',
};


/**
 * Mapping from ASCII value to value reported when control is pressed
 *
 * The table starts with ASCII value 32.
 */
static unsigned char control[256 - 32] = {
	/*  32 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  48 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  64 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  80 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	/*  96 */  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	/* 112 */ 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,  0,
};


/**
 * State machine that translates keycode sequences to terminal characters
 */
class Scancode_tracker
{
	private:

		/**
		 * Tables containing the scancode-to-character mapping
		 */
		unsigned char const *_keymap;
		unsigned char const *_shift;
		unsigned char const *_altgr;

		/**
		 * Current state of modifer keys
		 */
		bool _mod_shift;
		bool _mod_control;
		bool _mod_altgr;

		/**
		 * Currently pressed key, or 0 if no normal key (one that can be
		 * encoded in a single 'char') is pressed
		 */
		unsigned char _last_character;

		/**
		 * Currently pressed special key (a key that corresponds to an escape
		 * sequence), or no if no special key is pressed
		 */
		char const *_last_sequence;

		/**
		 * Convert keycode to terminal character
		 */
		unsigned char _keycode_to_latin1(int keycode)
		{
			if (keycode >= 112) return 0;

			unsigned ch = _keymap[keycode];

			if (ch < 32)
				return ch;

			/* all ASCII-to-ASCII table start at index 32 */
			if (_mod_shift || _mod_control || _mod_altgr) {
				ch -= 32;

				/*
				 * 'ch' is guaranteed to be in the range 0..223. So it is safe to
				 * use it as index into the ASCII-to-ASCII tables.
				 */

				if (_mod_shift)
					return _shift[ch];

				if (_mod_control)
					return control[ch];

				if (_altgr && _mod_altgr)
					return _altgr[ch];
			}

			return ch;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param keymap table for keycode-to-character mapping
		 * \param shift  table for character-to-character mapping used when
		 *               Shift is pressed
		 * \param altgr  table for character-to-character mapping with AltGr
		 *               is pressed
		 */
		Scancode_tracker(unsigned char const *keymap,
		                 unsigned char const *shift,
		                 unsigned char const *altgr)
		:
			_keymap(keymap),
			_shift(shift),
			_altgr(altgr),
			_mod_shift(false),
			_mod_control(false),
			_mod_altgr(false),
			_last_character(0),
			_last_sequence(0)
		{ };

		/**
		 * Submit key event to state machine
		 *
		 * \param press  true on press event, false on release event
		 */
		void submit(int keycode, bool press)
		{
			/* track modifier keys */
			switch (keycode) {
			case Input::KEY_LEFTSHIFT:
			case Input::KEY_RIGHTSHIFT:
				_mod_shift = press;
				break;

			case Input::KEY_LEFTCTRL:
			case Input::KEY_RIGHTCTRL:
				_mod_control = press;
				break;

			case Input::KEY_RIGHTALT:
				_mod_altgr = press;

			default:
				break;
			}

			/* reset information about the currently pressed key */
			_last_character = 0;
			_last_sequence  = 0;

			if (!press) return;

			/* convert key codes to ASCII */
			_last_character = _keycode_to_latin1(keycode);

			/* handle special key to be represented by an escape sequence */
			if (!_last_character) {
				switch (keycode) {
				case Input::KEY_DOWN:     _last_sequence = "\E[B";   break;
				case Input::KEY_UP:       _last_sequence = "\E[A";   break;
				case Input::KEY_RIGHT:    _last_sequence = "\E[C";   break;
				case Input::KEY_LEFT:     _last_sequence = "\E[D";   break;
				case Input::KEY_HOME:     _last_sequence = "\E[1~";  break;
				case Input::KEY_INSERT:   _last_sequence = "\E[2~";  break;
				case Input::KEY_DELETE:   _last_sequence = "\E[3~";  break;
				case Input::KEY_END:      _last_sequence = "\E[4~";  break;
				case Input::KEY_PAGEUP:   _last_sequence = "\E[5~";  break;
				case Input::KEY_PAGEDOWN: _last_sequence = "\E[6~";  break;
				case Input::KEY_F1:       _last_sequence = "\E[[A";  break;
				case Input::KEY_F2:       _last_sequence = "\E[[B";  break;
				case Input::KEY_F3:       _last_sequence = "\E[[C";  break;
				case Input::KEY_F4:       _last_sequence = "\E[[D";  break;
				case Input::KEY_F5:       _last_sequence = "\E[[E";  break;
				case Input::KEY_F6:       _last_sequence = "\E[17~"; break;
				case Input::KEY_F7:       _last_sequence = "\E[18~"; break;
				case Input::KEY_F8:       _last_sequence = "\E[19~"; break;
				case Input::KEY_F9:       _last_sequence = "\E[20~"; break;
				case Input::KEY_F10:      _last_sequence = "\E[21~"; break;
				case Input::KEY_F11:      _last_sequence = "\E[23~"; break;
				case Input::KEY_F12:      _last_sequence = "\E[24~"; break;
				}
			}
		}

		/**
		 * Output currently pressed key to read buffer
		 */
		void emit_current_character(Read_buffer &read_buffer)
		{
			if (_last_character)
				read_buffer.add(_last_character);

			if (_last_sequence)
				read_buffer.add(_last_sequence);
		}

		/**
		 * Return true if there is a currently pressed key
		 */
		bool valid() const
		{
			return (_last_sequence || _last_character);
		}
};


int main(int, char **)
{
	using namespace Genode;

	PDBG("--- terminal service started ---");

	static Framebuffer::Connection framebuffer;
	static Input::Connection       input;
	static Timer::Connection       timer;
	static Cap_connection          cap;

	Dataspace_capability ev_ds_cap = input.dataspace();

	Input::Event *ev_buf = static_cast<Input::Event *>
	                       (env()->rm_session()->attach(ev_ds_cap));

	/* initialize entry point that serves the root interface */
	enum { STACK_SIZE = sizeof(addr_t)*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "terminal_ep");

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/* input read buffer */
	static Read_buffer read_buffer;

	/* initialize color palette */
	color_palette[0] = Color(  0,   0,   0);	/* black */
	color_palette[1] = Color(255,   0,   0);	/* red */
	color_palette[2] = Color(  0, 255,   0);	/* green */
	color_palette[3] = Color(255, 255,   0);	/* yellow */
	color_palette[4] = Color(  0,   0, 255);	/* blue */
	color_palette[5] = Color(255,   0, 255);	/* magenta */
	color_palette[6] = Color(  0, 255, 255);	/* cyan */
	color_palette[7] = Color(255, 255, 255);	/* white */

	/* the upper portion of the palette contains highlight colors */
	for (int i = 0; i < 8; i++) {
		Color col = color_palette[i];
		col = Color((col.r*2)/3, (col.g*2)/3, (col.b*2)/3);
		color_palette[i + 8] = col;
	}

	static Terminal::Flush_callback_registry flush_callback_registry;

	/* create root interface for service */
	static Terminal::Root_component root(&ep, &sliced_heap,
	                                     &read_buffer, &framebuffer,
	                                     flush_callback_registry);

	/* announce service at our parent */
	env()->parent()->announce(ep.manage(&root));

	/* state needed for key-repeat handling */
	static int const repeat_delay = 170;
	static int const repeat_rate  =  25;
	static int       repeat_cnt   =   0;

	unsigned char *keymap = usenglish_keymap;
	unsigned char *shift  = usenglish_shift;
	unsigned char *altgr  = 0;

	/*
	 * Read keyboard layout from config file
	 */
	try {
		using namespace Genode;

		if (config()->xml_node().sub_node("keyboard")
		                        .attribute("layout").has_value("de")) {

			keymap = german_keymap;
			shift  = german_shift;
			altgr  = german_altgr;
		}
	} catch (...) { }

	static Scancode_tracker scancode_tracker(keymap, shift, altgr);

	while (1) {

		flush_callback_registry.flush();

		while (!input.is_pending()) {
			enum { PASSED_MSECS = 10 };
			timer.msleep(PASSED_MSECS);

			flush_callback_registry.flush();

			if (scancode_tracker.valid()) {
				repeat_cnt -= PASSED_MSECS;

				if (repeat_cnt < 0) {

					/* repeat current character or sequence */
					scancode_tracker.emit_current_character(read_buffer);

					/* reset repeat counter according to repeat rate */
					repeat_cnt = repeat_rate;
				}
			}
		}

		unsigned num_events = input.flush();

		for (Input::Event *event = ev_buf; num_events--; event++) {

			bool press   = (event->type() == Input::Event::PRESS   ? true : false);
			bool release = (event->type() == Input::Event::RELEASE ? true : false);
			int  keycode =  event->keycode();

			if (press || release)
				scancode_tracker.submit(keycode, press);

			if (press)
				scancode_tracker.emit_current_character(read_buffer);

			/* setup first key repeat */
			repeat_cnt = repeat_delay;
		}
	}
	return 0;
}
