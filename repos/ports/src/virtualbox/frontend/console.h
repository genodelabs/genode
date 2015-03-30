/*
 * \brief  Console implementation of VirtualBox for Genode
 * \author Alexander Boettcher
 * \author Norman Feske
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
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
#include "ConsoleImpl.h"
#include <base/printf.h>
#include <os/attached_dataspace.h>
#include <report_session/connection.h>
#include <vbox_pointer/shape_report.h>


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


class GenodeConsole : public Console {

	private:

		Input::Connection           _input;
		Genode::Signal_receiver     _receiver;
		Genode::Signal_context      _context;
		Input::Event               *_ev_buf;
		unsigned                    _ax, _ay;
		bool                        _last_received_motion_event_was_absolute;
		Report::Connection          _shape_report_connection;
		Genode::Attached_dataspace  _shape_report_ds;
		Vbox_pointer::Shape_report *_shape_report;

		bool _key_status[Input::KEY_MAX + 1];

		static bool _is_mouse_button(Input::Keycode keycode)
		{
			return keycode == Input::BTN_LEFT
			    || keycode == Input::BTN_RIGHT
			    || keycode == Input::BTN_MIDDLE;
		}

	public:

		GenodeConsole()
		:
			Console(),
			_ev_buf(static_cast<Input::Event *>(Genode::env()->rm_session()->attach(_input.dataspace()))),
			_ax(0), _ay(0),
			_last_received_motion_event_was_absolute(false),
			_shape_report_connection("shape", sizeof(Vbox_pointer::Shape_report)),
			_shape_report_ds(_shape_report_connection.dataspace()),
			_shape_report(_shape_report_ds.local_addr<Vbox_pointer::Shape_report>())
		{
			for (unsigned i = 0; i <= Input::KEY_MAX; i++)
				_key_status[i] = 0;

			_input.sigh(_receiver.manage(&_context));
		}

		void eventWait(IKeyboard * gKeyboard, IMouse * gMouse)
		{
			static LONG64 mt_events [64];
			unsigned      mt_number = 0;

			_receiver.wait_for_signal();

			/* read out input capabilities of guest */
			bool guest_abs = false, guest_rel = false, guest_multi = false;
			gMouse->COMGETTER(AbsoluteSupported)(&guest_abs);
			gMouse->COMGETTER(RelativeSupported)(&guest_rel);
			gMouse->COMGETTER(MultiTouchSupported)(&guest_multi);

			for (int i = 0, num_ev = _input.flush(); i < num_ev; ++i) {
				Input::Event &ev = _ev_buf[i];

				bool const is_press   = ev.type() == Input::Event::PRESS;
				bool const is_release = ev.type() == Input::Event::RELEASE;
				bool const is_key     = is_press || is_release;
				bool const is_motion  = ev.type() == Input::Event::MOTION;
				bool const is_wheel   = ev.type() == Input::Event::WHEEL;
				bool const is_touch   = ev.type() == Input::Event::TOUCH;

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
					unsigned const buttons = (_key_status[Input::BTN_LEFT]   ? MouseButtonState_LeftButton : 0)
					                       | (_key_status[Input::BTN_RIGHT]  ? MouseButtonState_RightButton : 0)
					                       | (_key_status[Input::BTN_MIDDLE] ? MouseButtonState_MiddleButton : 0);
					if (ev.is_absolute_motion()) {

						_last_received_motion_event_was_absolute = true;

						/* transform absolute to relative if guest is so odd */
						if (!guest_abs && guest_rel) {
							int const boundary = 20;
							int rx = ev.ax() - _ax;
							int ry = ev.ay() - _ay;
							rx = Genode::min(boundary, Genode::max(-boundary, rx));
							ry = Genode::min(boundary, Genode::max(-boundary, ry));
							gMouse->PutMouseEvent(rx, ry, 0, 0, buttons);
						} else
							gMouse->PutMouseEventAbsolute(ev.ax(), ev.ay(), 0,
							                              0, buttons);

						_ax = ev.ax();
						_ay = ev.ay();

					} else if (ev.is_relative_motion()) {

						_last_received_motion_event_was_absolute = false;

						/* prefer relative motion event */
						if (guest_rel)
							gMouse->PutMouseEvent(ev.rx(), ev.ry(), 0, 0, buttons);
						else if (guest_abs) {
							_ax += ev.rx();
							_ay += ev.ry();
							gMouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
						}
					}
					/* only the buttons changed */
					else {

						if (_last_received_motion_event_was_absolute) {
							/* prefer absolute button event */
							if (guest_abs)
								gMouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
							else if (guest_rel)
								gMouse->PutMouseEvent(0, 0, 0, 0, buttons);
						} else {
							/* prefer relative button event */
							if (guest_rel)
								gMouse->PutMouseEvent(0, 0, 0, 0, buttons);
							else if (guest_abs)
								gMouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
						}

					}
				}

				if (is_wheel)
					gMouse->PutMouseEvent(0, 0, ev.rx(), ev.ry(), 0);

				if (is_touch) {
					/* if multitouch queue is full - send it */
					if (mt_number >= sizeof(mt_events) / sizeof(mt_events[0])) {
						gMouse->PutEventMultiTouch(mt_number, mt_number,
						                           mt_events, RTTimeMilliTS());
						mt_number = 0;
					}

					int x    = ev.ax();
					int y    = ev.ay();
					int slot = ev.code();

					/* Mouse::putEventMultiTouch drops values of 0 */
					if (x <= 0) x = 1;
					if (y <= 0) y = 1;

					enum MultiTouch {
						None = 0x0,
						InContact = 0x01,
						InRange = 0x02
					};

					int status = MultiTouch::InContact | MultiTouch::InRange;
					if (ev.is_touch_release())
						status = MultiTouch::None;

					uint16_t const s = RT_MAKE_U16(slot, status);
					mt_events[mt_number++] = RT_MAKE_U64_FROM_U16(x, y, s, 0);
				}

			}

			/* if there are elements - send it */
			if (mt_number)
				gMouse->PutEventMultiTouch(mt_number, mt_number, mt_events,
				                           RTTimeMilliTS());
		}

		void onMouseCapabilityChange(BOOL supportsAbsolute, BOOL supportsRelative,
		                             BOOL supportsMT, BOOL needsHostCursor)
		{
			if (supportsAbsolute) {
				/* let the guest hide the software cursor */
				Mouse *gMouse = getMouse();
				gMouse->PutMouseEventAbsolute(-1, -1, 0, 0, 0);
			}
		}

		void onMousePointerShapeChange(bool fVisible, bool fAlpha,
		                               uint32_t xHot, uint32_t yHot,
		                               uint32_t width, uint32_t height,
		                               ComSafeArrayIn(BYTE,pShape))
		{
			com::SafeArray<BYTE> shape_array(ComSafeArrayInArg(pShape));

			if (fVisible && ((width == 0) || (height == 0)))
				return;

			_shape_report->visible = fVisible;
			_shape_report->x_hot = xHot;
			_shape_report->y_hot = yHot;
			_shape_report->width = width;
			_shape_report->height = height;

			unsigned int and_mask_size = (_shape_report->width + 7) / 8 *
			                              _shape_report->height;

			unsigned char *and_mask = shape_array.raw();

			unsigned char *shape = and_mask + ((and_mask_size + 3) & ~3);

			size_t shape_size = shape_array.size() - (shape - and_mask);

			if (shape_size > Vbox_pointer::MAX_SHAPE_SIZE) {
				PERR("%s: shape data buffer is too small for %zu bytes",
				     __func__, shape_size);
				return;
			}

			Genode::memcpy(_shape_report->shape,
			               shape,
			               shape_size);

			if (fVisible && !fAlpha) {

				for (unsigned int i = 0; i < width * height; i++) {

					unsigned int *color =
						&((unsigned int*)_shape_report->shape)[i];

					/* heuristic from VBoxSDL.cpp */

					if (and_mask[i / 8] & (1 << (7 - (i % 8)))) {

						if (*color & 0x00ffffff)
							*color = 0xff000000;
						else
							*color = 0x00000000;

					} else
						*color |= 0xff000000;
				}
			}

			_shape_report_connection.submit(sizeof(Vbox_pointer::Shape_report));
		}
};
