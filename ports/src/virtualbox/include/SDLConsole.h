/*
 * \brief  Console implementation of VirtualBox for Genode
 * \author Alexander Boettcher
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <input/event.h>
#include <input/keycodes.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>

/* included from os/src/drivers/input/ps2 */
#include <scan_code_set_1.h>

/* VirtualBox includes */
#include <ConsoleImpl.h>
#include <base/printf.h>

/* XXX */
enum { KMOD_RCTRL = 0, SDLK_RCTRL = 0 };


class Scan_code
{
	private:

		class Converter
		{
			public:

				unsigned char scan_code     [Input::KEY_UNKNOWN];
				unsigned char scan_code_ext [Input::KEY_UNKNOWN];

			private:

				unsigned char _search_scan_code(Input::Keycode keycode)
				{
					for (unsigned i = 0; i < SCAN_CODE_SET_1_NUM_KEYS; i++)
						if (scan_code_set_1[i] == keycode)
							return i;
					return 0;
				}

				unsigned char _search_scan_code_ext(Input::Keycode keycode)
				{
					for (unsigned i = 0; i < SCAN_CODE_SET_1_NUM_KEYS; i++)
						if (scan_code_set_1_0xe0[i] == keycode)
							return i;
					return 0;
				}

			public:

				Converter()
				{
					init_scan_code_set_1_0xe0();

					for (unsigned i = 0; i < Input::KEY_UNKNOWN; i++) {
						scan_code     [i] = _search_scan_code    ((Input::Keycode)i);
						scan_code_ext [i] = _search_scan_code_ext((Input::Keycode)i);
					}
				}
		};

		static Converter &converter()
		{
			static Converter inst;
			return inst;
		}

		Input::Keycode _keycode;

	public:

		Scan_code(Input::Keycode keycode) : _keycode(keycode) { }

		bool is_normal() const { return converter().scan_code[_keycode]; }
		bool is_ext()    const { return converter().scan_code_ext[_keycode]; }

		bool valid() const
		{
			return is_normal() || is_ext();
		}

		unsigned char code() const
		{
			return converter().scan_code[_keycode];
		}

		unsigned char ext() const
		{
			return converter().scan_code_ext[_keycode];
		}
};


class SDLConsole : public Console {

	private:

		Timer::Connection  timer;
		Input::Connection  input;
		Input::Event      *_ev_buf;
		unsigned           _ax, _ay;

		bool _key_status[Input::KEY_MAX + 1];

		static bool _is_mouse_button(Input::Keycode keycode)
		{
			return keycode == Input::BTN_LEFT
			    || keycode == Input::BTN_RIGHT
			    || keycode == Input::BTN_MIDDLE;
		}

	public:

		SDLConsole()
		:
			Console(),
			_ev_buf(static_cast<Input::Event *>(Genode::env()->rm_session()->attach(input.dataspace()))),
			_ax(0), _ay(0)
		{
			for (unsigned i = 0; i <= Input::KEY_MAX; i++)
				_key_status[i] = 0;

			if (FAILED(gMouse->init(this))) {
				PERR("mouse init failed");
				return;
			}

			mfInitialized = true;
		}

		void updateTitlebar()
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void updateTitlebarProgress(const char *, int)
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void inputGrabStart()
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void inputGrabEnd()
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void mouseSendEvent(int)
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void onMousePointerShapeChange(bool, bool, uint32_t, uint32_t,
		                               uint32_t, uint32_t, void *)
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		void progressInfo(PVM, unsigned, void *)
		{
			PERR("%s:%s called", __FILE__, __FUNCTION__);
		}

		CONEVENT eventWait()
		{
			while (!input.is_pending()) { timer.msleep(50); }

			for (int i = 0, num_ev = input.flush(); i < num_ev; ++i) {
				Input::Event &ev = _ev_buf[i];

				bool const is_press   = ev.type() == Input::Event::PRESS;
				bool const is_release = ev.type() == Input::Event::RELEASE;
				bool const is_key     = is_press || is_release;
				bool const is_motion  = ev.type() == Input::Event::MOTION;

				if (is_key) {
					Scan_code scan_code(ev.keycode());

					unsigned char const release_bit =
						(ev.type() == Input::Event::RELEASE) ? 0x80 : 0;

					if (scan_code.is_normal())
						gKeyboard->PutScancode(scan_code.code() | release_bit);

					if (scan_code.is_ext()) {
						gKeyboard->PutScancode(0xe0);
						gKeyboard->PutScancode(scan_code.ext() | release_bit);
					}
				}

				/*
				 * Track press/release status of keys and buttons. Currently,
				 * only the mouse-button states are actually used.
				 */
				if (is_press)
					_key_status[ev.keycode()] = true;

				if (is_release)
					_key_status[ev.keycode()] = false;

				bool const is_mouse_button_event =
					is_key && _is_mouse_button(ev.keycode());

				bool const is_mouse_event = is_mouse_button_event || is_motion;

				if (is_mouse_event) {
					unsigned const buttons = (_key_status[Input::BTN_LEFT]   ? 0x1 : 0)
					                       | (_key_status[Input::BTN_RIGHT]  ? 0x2 : 0)
					                       | (_key_status[Input::BTN_MIDDLE] ? 0x4 : 0);

					if (ev.is_absolute_motion()) {
						int const rx = ev.ax() - _ax; _ax = ev.ax();
						int const ry = ev.ay() - _ay; _ay = ev.ay();
						gMouse->PutMouseEvent(rx, ry, 0, 0, buttons);
						gMouse->PutMouseEventAbsolute(ev.ax(), ev.ay(), 0, 0, buttons);
					} else if (ev.is_relative_motion())
						gMouse->PutMouseEvent(ev.rx(), ev.ry(), 0, 0, buttons);

					/* only the buttons changed */
					else
						gMouse->PutMouseEvent(0, 0, 0, 0, buttons);
				}
			}

			return CONEVENT_NONE;
		}

		void     eventQuit() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		void     resetKeys(void) { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		VMMDev   *getVMMDev() { /*PERR("%s:%s called", __FILE__, __FUNCTION__);*/ return 0; }
		Display  *getDisplay() { return gDisplay; }
};
