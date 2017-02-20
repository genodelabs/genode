/*
 * \brief  State machine for translating scan codes to characters
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__SCANCODE_TRACKER_H_
#define _TERMINAL__SCANCODE_TRACKER_H_

#include <input/keycodes.h>
#include <terminal/read_buffer.h>

namespace Terminal { class Scancode_tracker; }


/**
 * State machine that translates keycode sequences to terminal characters
 */
class Terminal::Scancode_tracker
{
	private:

		/**
		 * Tables containing the scancode-to-character mapping
		 */
		unsigned char const *_keymap;
		unsigned char const *_shift;
		unsigned char const *_altgr;
		unsigned char const *_control;

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
					return _control[ch];

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
		                 unsigned char const *altgr,
		                 unsigned char const *control)
		:
			_keymap(keymap),
			_shift(shift),
			_altgr(altgr),
			_control(control),
			_mod_shift(false),
			_mod_control(false),
			_mod_altgr(false),
			_last_character(0),
			_last_sequence(0)
		{ }

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

#endif /* _TERMINAL__SCANCODE_TRACKER_H_ */
