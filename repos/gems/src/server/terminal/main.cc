/*
 * \brief  Terminal service
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <input/event.h>
#include <nitpicker_gfx/tff_font.h>

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>

/* local includes */
#include "text_screen_surface.h"
#include "session.h"

namespace Terminal { struct Main; }


struct Terminal::Main : Character_consumer
{
	/*
	 * Noncopyable
	 */
	Main(Main const &);
	Main &operator = (Main const &);

	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	/**
	 * Return font data according to config
	 */
	static char const *_font_data(Xml_node config);

	Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };

	Reconstructible<Tff_font> _font { _font_data(_config.xml()), _glyph_buffer };

	Reconstructible<Font_family> _font_family { *_font };

	Color_palette _color_palette { };

	void _handle_config();

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Input::Connection _input { _env };
	Timer::Connection _timer { _env };

	Heap _heap { _env.ram(), _env.rm() };

	Framebuffer _framebuffer { _env, _config_handler };

	typedef Pixel_rgb565 PT;

	Constructible<Text_screen_surface<PT>> _text_screen_surface { };

	Session::Size _terminal_size { };

	/*
	 * Time in milliseconds between a change of the terminal content and the
	 * update of the pixels. By delaying the update, multiple intermediate
	 * changes result in only one rendering step.
	 */
	unsigned const _flush_delay = 5;

	bool _flush_scheduled = false;

	void _handle_flush(Duration)
	{
		_flush_scheduled = false;

		// XXX distinguish between normal and alternate display
		if (_text_screen_surface.constructed())
			_text_screen_surface->redraw();
	}

	Timer::One_shot_timeout<Main> _flush_timeout {
		_timer, *this, &Main::_handle_flush };

	void _schedule_flush()
	{
		if (!_flush_scheduled) {
			_flush_timeout.schedule(Microseconds{1000*_flush_delay});
			_flush_scheduled = true;
		}
	}

	/**
	 * Character_consumer interface, called from 'Terminal::Session_component'
	 */
	void consume_character(Character c) override
	{
		// XXX distinguish between normal and alternative display mode (smcup)
		if (_text_screen_surface.constructed())
			_text_screen_surface->apply_character(c);

		_schedule_flush();
	}

	/* input read buffer */
	Read_buffer _read_buffer { };

	/* create root interface for service */
	Root_component _root { _env, _heap, _read_buffer, *this };

	void _handle_input();

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	Main(Env &env) : _env(env)
	{
		_handle_config();
		_config.sigh(_config_handler);

		_input.sigh(_input_handler);

		/* announce service at our parent */
		_env.parent().announce(_env.ep().manage(_root));
	}
};


/* built-in fonts */
extern char const _binary_notix_8_tff_start;
extern char const _binary_terminus_12_tff_start;
extern char const _binary_terminus_16_tff_start;


char const *Terminal::Main::_font_data(Xml_node config)
{
	if (config.has_sub_node("font")) {
		size_t const size = config.sub_node("font").attribute_value("size", 16U);

		switch (size) {
		case  8: return &_binary_notix_8_tff_start;     break;
		case 12: return &_binary_terminus_12_tff_start; break;
		case 16: return &_binary_terminus_16_tff_start; break;
		default: break;
		}
	}
	return &_binary_terminus_16_tff_start;
}


void Terminal::Main::_handle_config()
{
	_config.update();

	_font_family.destruct();
	_font.destruct();

	Xml_node const config = _config.xml();

	_font.construct(_font_data(config), _glyph_buffer);
	_font_family.construct(*_font);

	/*
	 * Adapt terminal to framebuffer mode changes
	 */
	_framebuffer.switch_to_new_mode();
	_text_screen_surface.construct(_heap, *_font_family,
	                               _color_palette, _framebuffer);
	_terminal_size = _text_screen_surface->size();
	_root.notify_resized(_terminal_size);
	_schedule_flush();
}


void Terminal::Main::_handle_input()
{
	_input.for_each_event([&] (Input::Event const &event) {

		if (event.type() == Input::Event::CHARACTER) {
			Input::Event::Utf8 const utf8 = event.utf8();

			char const sequence[] { (char)utf8.b0, (char)utf8.b1,
			                        (char)utf8.b2, (char)utf8.b3, 0 };

			/* function-key unicodes */
			enum {
				CODEPOINT_UP     = 0xf700, CODEPOINT_DOWN     = 0xf701,
				CODEPOINT_LEFT   = 0xf702, CODEPOINT_RIGHT    = 0xf703,
				CODEPOINT_F1     = 0xf704, CODEPOINT_F2       = 0xf705,
				CODEPOINT_F3     = 0xf706, CODEPOINT_F4       = 0xf707,
				CODEPOINT_F5     = 0xf708, CODEPOINT_F6       = 0xf709,
				CODEPOINT_F7     = 0xf70a, CODEPOINT_F8       = 0xf70b,
				CODEPOINT_F9     = 0xf70c, CODEPOINT_F10      = 0xf70d,
				CODEPOINT_F11    = 0xf70e, CODEPOINT_F12      = 0xf70f,
				CODEPOINT_HOME   = 0xf729, CODEPOINT_INSERT   = 0xf727,
				CODEPOINT_DELETE = 0xf728, CODEPOINT_END      = 0xf72b,
				CODEPOINT_PAGEUP = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
			};

			Codepoint const codepoint = Utf8_ptr(sequence).codepoint();

			char const *special_sequence = nullptr;
			switch (codepoint.value) {
			case CODEPOINT_UP:       special_sequence = "\EOA";   break;
			case CODEPOINT_DOWN:     special_sequence = "\EOB";   break;
			case CODEPOINT_LEFT:     special_sequence = "\EOD";   break;
			case CODEPOINT_RIGHT:    special_sequence = "\EOC";   break;
			case CODEPOINT_F1:       special_sequence = "\EOP";   break;
			case CODEPOINT_F2:       special_sequence = "\EOQ";   break;
			case CODEPOINT_F3:       special_sequence = "\EOR";   break;
			case CODEPOINT_F4:       special_sequence = "\EOS";   break;
			case CODEPOINT_F5:       special_sequence = "\E[15~"; break;
			case CODEPOINT_F6:       special_sequence = "\E[17~"; break;
			case CODEPOINT_F7:       special_sequence = "\E[18~"; break;
			case CODEPOINT_F8:       special_sequence = "\E[19~"; break;
			case CODEPOINT_F9:       special_sequence = "\E[20~"; break;
			case CODEPOINT_F10:      special_sequence = "\E[21~"; break;
			case CODEPOINT_F11:      special_sequence = "\E[23~"; break;
			case CODEPOINT_F12:      special_sequence = "\E[24~"; break;
			case CODEPOINT_HOME:     special_sequence = "\E[1~";  break;
			case CODEPOINT_INSERT:   special_sequence = "\E[2~";  break;
			case CODEPOINT_DELETE:   special_sequence = "\E[3~";  break;
			case CODEPOINT_END:      special_sequence = "\E[4~";  break;
			case CODEPOINT_PAGEUP:   special_sequence = "\E[5~";  break;
			case CODEPOINT_PAGEDOWN: special_sequence = "\E[6~";  break;
			};

			if (special_sequence)
				_read_buffer.add(special_sequence);
			else
				_read_buffer.add(sequence);
		}
	});
}


void Component::construct(Genode::Env &env) { static Terminal::Main main(env); }
