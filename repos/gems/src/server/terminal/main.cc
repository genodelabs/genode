/*
 * \brief  Terminal service
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/heap.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <os/attached_ram_dataspace.h>
#include <input/event.h>
#include <os/config.h>
#include <util/color.h>
#include <os/pixel_rgb565.h>

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>
#include <terminal/scancode_tracker.h>
#include <terminal/keymaps.h>
#include <terminal/char_cell_array_character_screen.h>
#include <terminal_session/terminal_session.h>

/* nitpicker graphic back end */
#include <nitpicker_gfx/text_painter.h>


using Genode::Pixel_rgb565;
typedef Text_painter::Font Font;


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


using Genode::Color;


static Color color_palette[2*8];


static Color foreground_color(Char_cell const &cell)
{
	Color col = color_palette[cell.colidx_fg() + (cell.highlight() ? 8 : 0)];

	if (cell.inverse())
		col = Color(col.r/2, col.g/2, col.b/2);

	return col;
}


static Color background_color(Char_cell const &cell)
{
	Color col = color_palette[cell.colidx_bg() + (cell.highlight() ? 8 : 0)];

	if (cell.inverse())
		return Color((col.r + 255)/2, (col.g + 255)/2, (col.b + 255)/2);

	return col;
}


class Font_family
{
	private:

		Font const &_regular;

	public:

		Font_family(Font const &regular /* ...to be extended */ )
		: _regular(regular) { }

		/**
		 * Return font for specified face
		 *
		 * For now, we do not support font faces other than regular.
		 */
		Font const *font(Font_face) const { return &_regular; }

		unsigned cell_width()  const { return _regular.str_w("m"); }
		unsigned cell_height() const { return _regular.str_h("m"); }
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
	Font const &regular_font = *font_family.font(Font_face::REGULAR);
	unsigned glyph_height = regular_font.img_h,
	         glyph_step_x = regular_font.wtab['m'];

	unsigned y = 0;
	for (unsigned line = 0; line < cell_array->num_lines(); line++) {

		if (cell_array->line_dirty(line)) {

			if (verbose)
				Genode::log("convert line ", line);

			unsigned x = 0;
			for (unsigned column = 0; column < cell_array->num_cols(); column++) {

				Char_cell      cell  = cell_array->get_cell(column, line);
				Font const    *font  = font_family.font(cell.font_face());
				unsigned char  ascii = cell.ascii;

				if (ascii == 0)
					ascii = ' ';

				unsigned char const *glyph_base  = font->img + font->otab[ascii];

				unsigned glyph_width = regular_font.wtab[ascii];

				if (x + glyph_width > fb_width)	break;

				Color fg_color = foreground_color(cell);
				Color bg_color = background_color(cell);

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
					Genode::error("color mode ", _fb_mode, " not supported");
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

				log("new terminal session:");
				log("  framebuffer has mode ", _fb_mode);
				log("  character size is ", _char_width, "x", _char_height, " pixels");
				log("  terminal size is ", _columns, "x", _lines, " characters");

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
				if (num_dirty_lines > 0)
					_framebuffer->refresh(0, first_dirty_line*_char_height,
					                      _fb_mode.width(),
					                      num_dirty_lines*_char_height);
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
					dst[num_bytes++] = _read_buffer->get();
				} while (!_read_buffer->empty() && num_bytes < dst_size);

				return num_bytes;
			}

			void _write(Genode::size_t num_bytes)
			{
				Genode::Lock::Guard guard(_lock);

				unsigned char *src = _io_buffer.local_addr<unsigned char>();

				for (unsigned i = 0; i < num_bytes; i++) {

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
				Genode::log("create terminal session");

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

	log("--- terminal service started ---");

	static Framebuffer::Connection framebuffer;
	static Input::Connection       input;
	static Timer::Connection       timer;
	static Cap_connection          cap;

	/* initialize entry point that serves the root interface */
	enum { STACK_SIZE = 2*sizeof(addr_t)*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "terminal_ep");

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/* input read buffer */
	static Terminal::Read_buffer read_buffer;

	/* initialize color palette */
	color_palette[0] = Color(  0,   0,   0);  /* black */
	color_palette[1] = Color(255, 128, 128);  /* red */
	color_palette[2] = Color(128, 255, 128);  /* green */
	color_palette[3] = Color(255, 255,   0);  /* yellow */
	color_palette[4] = Color(128, 128, 255);  /* blue */
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

	log("cell size is ", (int)font_family.cell_width(),
	    "x", (int)font_family.cell_height());

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

		while (!input.pending()) {
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

		input.for_each_event([&] (Input::Event const &event) {
			bool press   = (event.type() == Input::Event::PRESS   ? true : false);
			bool release = (event.type() == Input::Event::RELEASE ? true : false);
			int  keycode =  event.code();

			if (press || release)
				scancode_tracker.submit(keycode, press);

			if (press)
				scancode_tracker.emit_current_character(read_buffer);

			/* setup first key repeat */
			repeat_cnt = repeat_delay;
		});
	}
	return 0;
}
