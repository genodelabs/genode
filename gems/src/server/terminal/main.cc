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
#include <os/attached_ram_dataspace.h>
#include <input/event.h>
#include <os/config.h>

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>
#include <terminal/scancode_tracker.h>
#include <terminal/keymaps.h>
#include <terminal/cell_array.h>
#include <terminal_session/terminal_session.h>

/* nitpicker graphic back end */
#include <nitpicker_gfx/font.h>
#include <nitpicker_gfx/color.h>
#include <nitpicker_gfx/pixel_rgb565.h>


static bool const verbose = false;


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


static Color color_palette[2*8];


class Font_family
{
	private:

		Font const &_regular;

	public:

		enum Face { REGULAR, ITALIC, BOLD, BOLD_ITALIC };

		Font_family(Font const &regular /* ...to be extended */ )
		: _regular(regular) { }

		/**
		 * Return font for specified face
		 *
		 * For now, we do not support font faces other than regular.
		 */
		Font const *font(Face) const { return &_regular; }

		unsigned cell_width()  const { return _regular.str_w("m"); }
		unsigned cell_height() const { return _regular.str_h("m"); }
};


struct Char_cell
{
	unsigned char attr;
	unsigned char ascii;
	unsigned char color;

	enum { ATTR_COLIDX_MASK = 0x07,
	       ATTR_CURSOR      = 0x10,
	       ATTR_INVERSE     = 0x20,
	       ATTR_HIGHLIGHT   = 0x40 };

	enum { COLOR_MASK = 0x3f }; /* 111111 */

	Char_cell() : attr(0), ascii(0), color(0) { }

	Char_cell(unsigned char c, Font_family::Face f,
	          int colidx, bool inv, bool highlight)
	:
		attr(f | (inv ? ATTR_INVERSE : 0)
		       | (highlight ? ATTR_HIGHLIGHT : 0)),
		ascii(c),
		color(colidx & COLOR_MASK)
	{ }

	Font_family::Face font_face() const { return (Font_family::Face)(attr & 0x3); }

	int  colidx_fg() const { return color        & ATTR_COLIDX_MASK; }
	int  colidx_bg() const { return (color >> 3) & ATTR_COLIDX_MASK; }
	bool inverse()   const { return attr         & ATTR_INVERSE;     }
	bool highlight() const { return attr         & ATTR_HIGHLIGHT;   }

	Color fg_color() const
	{
		Color col = color_palette[colidx_fg() + (highlight() ? 8 : 0)];

		if (inverse())
			col = Color(col.r/2, col.g/2, col.b/2);

		return col;
	}

	Color bg_color() const
	{
		Color col = color_palette[colidx_bg() + (highlight() ? 8 : 0)];

		if (inverse())
			return Color((col.r + 255)/2, (col.g + 255)/2, (col.b + 255)/2);

		return col;
	}

	void set_cursor()   { attr |=  ATTR_CURSOR; }
	void clear_cursor() { attr &= ~ATTR_CURSOR; }

	bool has_cursor() const { return attr & ATTR_CURSOR; }
};


class Char_cell_array_character_screen : public Terminal::Character_screen
{
	private:

		enum Cursor_visibility { CURSOR_INVISIBLE,
		                         CURSOR_VISIBLE,
		                         CURSOR_VERY_VISIBLE };

		Cell_array<Char_cell> &_char_cell_array;
		Terminal::Boundary     _boundary;
		Terminal::Position     _cursor_pos;
		/**
		 * Color index contains the fg color in the first 3 bits
		 * and the bg color in the second 3 bits (0bbbbfff).
		 */
		int                    _color_index;
		bool                   _inverse;
		bool                   _highlight;
		Cursor_visibility      _cursor_visibility;
		int                    _region_start;
		int                    _region_end;
		int                    _tab_size;

		enum { DEFAULT_COLOR_INDEX_BG = 0, DEFAULT_COLOR_INDEX = 7, DEFAULT_TAB_SIZE = 8 };

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

		Char_cell_array_character_screen(Cell_array<Char_cell> &char_cell_array)
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
				                          Char_cell(c.ascii(), Font_family::REGULAR,
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

		void cuu1()   { PWRN("not implemented"); }
		void dch(int) { PDBG("not implemented"); }

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
				                          Char_cell(' ', Font_family::REGULAR,
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

		void el1() { PDBG("not implemented"); }

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

		void hts()    { PDBG("not implemented"); }
		void ich(int) { PDBG("not implemented"); }

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

		void oc() { PDBG("not implemented"); }

		void op()
		{
			_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
		}

		void rc()   { PDBG("not implemented"); }
		void ri()   { PDBG("not implemented"); }
		void ris()  { PDBG("not implemented"); }
		void rmam() { PDBG("not implemented"); }
		void rmir() { PDBG("not implemented"); }

		void setab(int value)
		{
			//_inverse = (value != 0);
			_color_index &= ~0x38; /* clear 111000 */
			_color_index |= (((value == 9) ? DEFAULT_COLOR_INDEX_BG : value) << 3);
		}

		void setaf(int value)
		{
			_color_index &= ~0x7; /* clear 000111 */
			_color_index |= (value == 9) ? DEFAULT_COLOR_INDEX : value;
		}

		void sgr(int value)
		{
			_highlight = (value & 0x1) != 0;
			_inverse   = (value & 0x2) != 0;

			/* sgr 0 is the command to reset all attributes, including color */
			if (value == 0)
				_color_index = DEFAULT_COLOR_INDEX | (DEFAULT_COLOR_INDEX_BG << 3);
		}

		void sgr0()
		{
			sgr(0);
		}

		void sc()         { PDBG("not implemented"); }
		void smam()       { PDBG("not implemented"); }
		void smir()       { PDBG("not implemented"); }
		void tbc()        { PDBG("not implemented"); }
		void u6(int, int) { PDBG("not implemented"); }
		void u7()         { PDBG("not implemented"); }
		void u8()         { PDBG("not implemented"); }
		void u9()         { PDBG("not implemented"); }

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
static void convert_char_array_to_pixels(Cell_array<Char_cell> *cell_array,
                                         PT                    *fb_base,
                                         unsigned               fb_width,
                                         unsigned               fb_height,
                                         Font_family const     &font_family)
{
	Font const &regular_font = *font_family.font(Font_family::REGULAR);
	unsigned glyph_height = regular_font.img_h,
	         glyph_step_x = regular_font.wtab['m'];

	unsigned y = 0;
	for (unsigned line = 0; line < cell_array->num_lines(); line++) {

		if (cell_array->line_dirty(line)) {

			if (verbose)
				Genode::printf("convert line %d\n", line);

			unsigned x = 0;
			for (unsigned column = 0; column < cell_array->num_cols(); column++) {

				Char_cell      cell  = cell_array->get_cell(column, line);
				Font const    *font  = font_family.font(cell.font_face());
				unsigned char  ascii = cell.ascii;

				if (ascii == 0)
					ascii = ' ';

				unsigned char const *glyph_base  = font->img + font->otab[ascii];

				unsigned glyph_width = regular_font.wtab[ascii];

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

			Cell_array<Char_cell>            _char_cell_array;
			Char_cell_array_character_screen _char_cell_array_character_screen;
			Terminal::Decoder                _decoder;

			Terminal::Position               _last_cursor_pos;

			Font_family const               *_font_family;

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
			                  Flush_callback_registry &flush_callback_registry,
			                  Font_family const       &font_family)
			:
				_read_buffer(read_buffer), _framebuffer(framebuffer),
				_flush_callback_registry(flush_callback_registry),
				_io_buffer(Genode::env()->ram_session(), io_buffer_size),
				_fb_mode(_framebuffer->mode()),
				_fb_ds_cap(_init_fb()),

				/* take size of space character as character cell size */
				_char_width(font_family.cell_width()),
				_char_height(font_family.cell_height()),

				/* compute number of characters fitting on the framebuffer */
				_columns(_fb_mode.width()/_char_width),
				_lines(_fb_mode.height()/_char_height),

				_fb_addr(Genode::env()->rm_session()->attach(_fb_ds_cap)),
				_char_cell_array(_columns, _lines, Genode::env()->heap()),
				_char_cell_array_character_screen(_char_cell_array),
				_decoder(_char_cell_array_character_screen),

				_font_family(&font_family)
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
				                                           _fb_mode.height(),
				                                          *_font_family);

				
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
			Font_family const       &_font_family;

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
					                                   _flush_callback_registry,
					                                   _font_family);
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
			               Flush_callback_registry &flush_callback_registry,
			               Font_family const       &font_family)
			:
				Genode::Root_component<Session_component>(ep, md_alloc),
				_read_buffer(read_buffer), _framebuffer(framebuffer),
				_flush_callback_registry(flush_callback_registry),
				_font_family(font_family)
			{ }
	};
}



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
	static Terminal::Read_buffer read_buffer;

	/* initialize color palette */
	color_palette[0] = Color(  0,   0,   0);  /* black */
	color_palette[1] = Color(255,   0,   0);  /* red */
	color_palette[2] = Color(  0, 255,   0);  /* green */
	color_palette[3] = Color(255, 255,   0);  /* yellow */
	color_palette[4] = Color(  0,   0, 255);  /* blue */
	color_palette[5] = Color(255,   0, 255);  /* magenta */
	color_palette[6] = Color(  0, 255, 255);  /* cyan */
	color_palette[7] = Color(255, 255, 255);  /* white */

	/* the upper portion of the palette contains highlight colors */
	for (int i = 0; i < 8; i++) {
		Color col = color_palette[i];
		col = Color((col.r*2)/3, (col.g*2)/3, (col.b*2)/3);
		color_palette[i + 8] = col;
	}

	/* built-in fonts */
	extern char _binary_notix_8_tff_start;
	extern char _binary_terminus_12_tff_start;
	extern char _binary_terminus_16_tff_start;

	/* pick font according to config file */
	char const *font_data = &_binary_terminus_16_tff_start;
	try {
		size_t font_size = 16;
		config()->xml_node().sub_node("font")
		                    .attribute("size").value(&font_size);

		switch (font_size) {
		case  8: font_data = &_binary_notix_8_tff_start;     break;
		case 12: font_data = &_binary_terminus_12_tff_start; break;
		case 16: font_data = &_binary_terminus_16_tff_start; break;
		default: break;
		}
	} catch (...) { }

	static Font font(font_data);
	static Font_family font_family(font);

	printf("cell size is %dx%d\n", (int)font_family.cell_width(),
	                               (int)font_family.cell_height());

	static Terminal::Flush_callback_registry flush_callback_registry;

	/* create root interface for service */
	static Terminal::Root_component root(&ep, &sliced_heap,
	                                     &read_buffer, &framebuffer,
	                                     flush_callback_registry,
	                                     font_family);

	/* announce service at our parent */
	env()->parent()->announce(ep.manage(&root));

	/* state needed for key-repeat handling */
	static int const repeat_delay = 170;
	static int const repeat_rate  =  25;
	static int       repeat_cnt   =   0;

	unsigned char *keymap = Terminal::usenglish_keymap;
	unsigned char *shift  = Terminal::usenglish_shift;
	unsigned char *altgr  = 0;

	/*
	 * Read keyboard layout from config file
	 */
	try {
		if (config()->xml_node().sub_node("keyboard")
		                        .attribute("layout").has_value("de")) {

			keymap = Terminal::german_keymap;
			shift  = Terminal::german_shift;
			altgr  = Terminal::german_altgr;
		}
	} catch (...) { }

	static Terminal::Scancode_tracker
		scancode_tracker(keymap, shift, altgr, Terminal::control);

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
