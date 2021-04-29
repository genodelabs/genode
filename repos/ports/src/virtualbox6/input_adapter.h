/*
 * \brief  Input adapter for VirtualBox main
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-12-11
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _INPUT_ADAPTER_H_
#define _INPUT_ADAPTER_H_

#include <scan_code.h>


struct Input_adapter
{
	struct Mouse
	{
		ComPtr<IMouse> _imouse;

		Mouse(ComPtr<IConsole> &iconsole)
		{
			attempt([&] () { return iconsole->COMGETTER(Mouse)(_imouse.asOutParam()); },
			        "unable to request mouse interface from console");
		}

		bool _key_status[Input::KEY_MAX + 1];

		typedef Genode::Surface_base::Point Point;

		Point _abs_pos { 0, 0 };

		bool _absolute { false };

		static bool _mouse_button(Input::Keycode keycode)
		{
			return keycode == Input::BTN_LEFT
			    || keycode == Input::BTN_RIGHT
			    || keycode == Input::BTN_MIDDLE
			    || keycode == Input::BTN_SIDE
			    || keycode == Input::BTN_EXTRA;
		}

		void handle_input_event(Input::Event const &);

		void absolute(bool absolute) { _absolute = absolute; }

	} _mouse;

	struct Keyboard
	{
		ComPtr<IKeyboard> _ikeyboard;

		Keyboard(ComPtr<IConsole> &iconsole)
		{
			attempt([&] () { return iconsole->COMGETTER(Keyboard)(_ikeyboard.asOutParam()); },
			        "unable to request keyboard interface from console");
		}

		void handle_input_event(Input::Event const &);

	} _keyboard;

	Input_adapter(ComPtr<IConsole> &iconsole)
	: _mouse(iconsole), _keyboard(iconsole) { }

	void handle_input_event(Input::Event const &);

	void mouse_absolute(bool absolute) { _mouse.absolute(absolute); }
};


void Input_adapter::Keyboard::handle_input_event(Input::Event const &ev)
{
	auto keyboard_submit = [&] (Input::Keycode key, bool release) {

		Scan_code scan_code(key);

		unsigned char const release_bit = release ? 0x80 : 0;

		if (scan_code.normal())
			_ikeyboard->PutScancode(scan_code.code() | release_bit);

		if (scan_code.ext()) {
			_ikeyboard->PutScancode(0xe0);
			_ikeyboard->PutScancode(scan_code.ext() | release_bit);
		}
	};

	ev.handle_press([&] (Input::Keycode key, Genode::Codepoint) {
		keyboard_submit(key, false); });

	ev.handle_release([&] (Input::Keycode key) {
		keyboard_submit(key, true); });
}


void Input_adapter::Mouse::handle_input_event(Input::Event const &ev)
{
	/* obtain bit mask of currently pressed mouse buttons */
	auto curr_mouse_button_bits = [&] () {
		return (_key_status[Input::BTN_LEFT]   ? MouseButtonState_LeftButton   : 0)
		     | (_key_status[Input::BTN_RIGHT]  ? MouseButtonState_RightButton  : 0)
		     | (_key_status[Input::BTN_MIDDLE] ? MouseButtonState_MiddleButton : 0)
		     | (_key_status[Input::BTN_SIDE]   ? MouseButtonState_XButton1     : 0)
		     | (_key_status[Input::BTN_EXTRA]  ? MouseButtonState_XButton2     : 0);
	};

	unsigned const old_mouse_button_bits = curr_mouse_button_bits();
	Point    const old_abs_pos           = _abs_pos;

	ev.handle_press([&] (Input::Keycode key, Genode::Codepoint) {
		if (_mouse_button(key))
			_key_status[key] = true; });

	ev.handle_release([&] (Input::Keycode key) {
		if (_mouse_button(key))
			_key_status[key] = false; });

	ev.handle_absolute_motion([&] (int ax, int ay) {
		_abs_pos = Point(ax, ay); });

	unsigned const mouse_button_bits = curr_mouse_button_bits();

	bool const abs_pos_changed = (old_abs_pos != _abs_pos);
	bool const buttons_changed = (old_mouse_button_bits != mouse_button_bits);

	if (abs_pos_changed || buttons_changed) {
		if (_absolute) {
			_imouse->PutMouseEventAbsolute(_abs_pos.x(), _abs_pos.y(), 0, 0, mouse_button_bits);
		} else {
			Point const rel = _abs_pos - old_abs_pos;

			_imouse->PutMouseEvent(rel.x(), rel.y(), 0, 0, mouse_button_bits);
		}
	}
}


void Input_adapter::handle_input_event(Input::Event const &ev)
{
	/* present the event to potential consumers */
	_keyboard.handle_input_event(ev);
	_mouse.handle_input_event(ev);
}

#endif /* _INPUT_ADAPTER_H_ */
