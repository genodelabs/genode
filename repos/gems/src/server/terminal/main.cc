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

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>
#include <terminal/scancode_tracker.h>
#include <terminal/keymaps.h>

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

	Reconstructible<Font>        _font        { _font_data(_config.xml()) };
	Reconstructible<Font_family> _font_family { *_font };

	unsigned char *_keymap = Terminal::usenglish_keymap;
	unsigned char *_shift  = Terminal::usenglish_shift;
	unsigned char *_altgr  = nullptr;

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

	/*
	 * builtin keyboard-layout handling
	 *
	 * \deprecated  The keyboard layout should be handled by the input-filter
	 *              component.
	 */
	Constructible<Scancode_tracker> _scancode_tracker { };

	/* state needed for key-repeat handling */
	unsigned const _repeat_delay = 250;
	unsigned const _repeat_rate  =  25;
	unsigned       _repeat_next  =   0;

	void _handle_input();

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	void _handle_key_repeat(Duration);

	Timer::One_shot_timeout<Main> _key_repeat_timeout {
		_timer, *this, &Main::_handle_key_repeat };

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

	_font.construct(_font_data(config));
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

	/*
	 * Read keyboard layout from config file
	 */

	_keymap = Terminal::usenglish_keymap;
	_shift  = Terminal::usenglish_shift;
	_altgr  = nullptr;

	if (config.has_sub_node("keyboard")) {

		if (config.sub_node("keyboard").attribute("layout").has_value("de")) {
			_keymap = Terminal::german_keymap;
			_shift  = Terminal::german_shift;
			_altgr  = Terminal::german_altgr;
		}

		if (config.sub_node("keyboard").attribute("layout").has_value("none")) {
			_keymap = nullptr;
			_shift  = nullptr;
			_altgr  = nullptr;
		}
	}

	if (!_scancode_tracker.constructed())
		_scancode_tracker.construct(_keymap, _shift, _altgr, Terminal::control);
}


void Terminal::Main::_handle_input()
{
	_input.for_each_event([&] (Input::Event const &event) {

	 	if (event.type() == Input::Event::CHARACTER) {
	 		Input::Event::Utf8 const utf8 = event.utf8();
	 		_read_buffer.add(utf8.b0);
	 		if (utf8.b1) _read_buffer.add(utf8.b1);
	 		if (utf8.b2) _read_buffer.add(utf8.b2);
	 		if (utf8.b3) _read_buffer.add(utf8.b3);
		}

		/* apply the terminal's built-in character map if configured */
		if (!_scancode_tracker.constructed())
			return;

		bool press   = (event.type() == Input::Event::PRESS   ? true : false);
		bool release = (event.type() == Input::Event::RELEASE ? true : false);
		int  keycode =  event.code();

		if (press || release)
			_scancode_tracker->submit(keycode, press);

		if (press) {
			_scancode_tracker->emit_current_character(_read_buffer);

			/* setup first key repeat */
			_repeat_next = _repeat_delay;
		}

		if (release)
			_repeat_next = 0;
	});

	if (_repeat_next)
		_key_repeat_timeout.schedule(Microseconds{1000*_repeat_next});
}


void Terminal::Main::_handle_key_repeat(Duration)
{
	if (_repeat_next) {

		/* repeat current character or sequence */
		if (_scancode_tracker.constructed())
			_scancode_tracker->emit_current_character(_read_buffer);

		_repeat_next = _repeat_rate;
	}

	_handle_input();
}


void Component::construct(Genode::Env &env) { static Terminal::Main main(env); }
