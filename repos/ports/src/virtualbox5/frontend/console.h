/*
 * \brief  Console implementation of VirtualBox for Genode
 * \author Alexander Boettcher
 * \author Norman Feske
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <input_session/connection.h>
#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <report_session/connection.h>
#include <timer_session/connection.h>

/* included from os/src/drivers/input/spec/ps2 */
#include <scan_code_set_1.h>

/* repos/ports includes */
#include <vbox_pointer/shape_report.h>

#include "../vmm.h"

/* VirtualBox includes */
#include "ConsoleImpl.h"

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

		bool normal() const { return converter().scan_code[_keycode]; }

		bool valid() const
		{
			return normal() || ext();
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

		Input::Connection                      _input;
		unsigned                               _ax, _ay;
		bool                                   _last_received_motion_event_was_absolute;
		Report::Connection                     _shape_report_connection;
		Genode::Attached_dataspace             _shape_report_ds;
		Vbox_pointer::Shape_report            *_shape_report;
		Genode::Reporter                      *_clipboard_reporter;
		Genode::Attached_rom_dataspace        *_clipboard_rom;
		IKeyboard                             *_vbox_keyboard;
		IMouse                                *_vbox_mouse;
		Genode::Signal_handler<GenodeConsole>  _input_signal_dispatcher;
		Genode::Signal_handler<GenodeConsole>  _mode_change_signal_dispatcher;
		Genode::Signal_handler<GenodeConsole>  _clipboard_signal_dispatcher;

		bool _key_status[Input::KEY_MAX + 1];

		static bool _mouse_button(Input::Keycode keycode)
		{
			return keycode == Input::BTN_LEFT
			    || keycode == Input::BTN_RIGHT
			    || keycode == Input::BTN_MIDDLE;
		}

	public:

		GenodeConsole()
		:
			Console(),
			_input(genode_env()),
			_ax(0), _ay(0),
			_last_received_motion_event_was_absolute(false),
			_shape_report_connection(genode_env(), "shape",
			                         sizeof(Vbox_pointer::Shape_report)),
			_shape_report_ds(genode_env().rm(), _shape_report_connection.dataspace()),
			_shape_report(_shape_report_ds.local_addr<Vbox_pointer::Shape_report>()),
			_clipboard_reporter(nullptr),
			_clipboard_rom(nullptr),
			_vbox_keyboard(0),
			_vbox_mouse(0),
			_input_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_input),
			_mode_change_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_mode_change),
			_clipboard_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_cb_rom_change)
		{
			for (unsigned i = 0; i <= Input::KEY_MAX; i++)
				_key_status[i] = 0;

			_input.sigh(_input_signal_dispatcher);
		}

		void init_clipboard();

		void init_backends(IKeyboard * gKeyboard, IMouse * gMouse);

		void i_onMouseCapabilityChange(BOOL supportsAbsolute,
		                               BOOL supportsRelative, BOOL supportsMT,
		                               BOOL needsHostCursor);

		void i_onMousePointerShapeChange(bool fVisible, bool fAlpha,
		                                 uint32_t xHot, uint32_t yHot,
		                                 uint32_t width, uint32_t height,
		                                 const uint8_t *pu8Shape,
		                                 uint32_t cbShape)
		{
			if (fVisible && ((width == 0) || (height == 0)))
				return;

			_shape_report->visible = fVisible;
			_shape_report->x_hot = xHot;
			_shape_report->y_hot = yHot;
			_shape_report->width = width;
			_shape_report->height = height;

			unsigned int and_mask_size = (_shape_report->width + 7) / 8 *
			                              _shape_report->height;

			const unsigned char *and_mask = pu8Shape;

			const unsigned char *shape = and_mask + ((and_mask_size + 3) & ~3);

			size_t shape_size = cbShape - (shape - and_mask);

			if (shape_size > Vbox_pointer::MAX_SHAPE_SIZE) {
				Genode::error(__func__, ": shape data buffer is too small for ",
				              shape_size, " bytes");
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

		void update_video_mode();

		void handle_input();
		void handle_mode_change();
		void handle_cb_rom_change();
};
