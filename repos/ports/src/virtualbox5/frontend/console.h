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
#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <input/event.h>
#include <input/keycodes.h>
#define Framebuffer Fb_Genode
#include <gui_session/connection.h>
#undef Framebuffer
#include <os/reporter.h>
#include <report_session/connection.h>
#include <timer_session/connection.h>

/* included from os/src/driver/ps2 */
#include <scan_code_set_1.h>

/* repos/ports includes */
#include <pointer/shape_report.h>

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

		Gui::Connection                        _gui;
		Input::Session_client                 &_input;
		unsigned                               _ax, _ay;
		bool                                   _last_received_motion_event_was_absolute;
		Report::Connection                     _shape_report_connection;
		Genode::Attached_dataspace             _shape_report_ds;
		Genode::Constructible<Genode::Attached_rom_dataspace> _caps_lock;
		Pointer::Shape_report                 *_shape_report;
		Genode::Reporter                      *_clipboard_reporter;
		Genode::Attached_rom_dataspace        *_clipboard_rom;
		IKeyboard                             *_vbox_keyboard;
		IMouse                                *_vbox_mouse;
		Genode::Signal_handler<GenodeConsole>  _input_signal_dispatcher;
		Genode::Signal_handler<GenodeConsole>  _mode_change_signal_dispatcher;
		Genode::Signal_handler<GenodeConsole>  _clipboard_signal_dispatcher;
		Genode::Signal_handler<GenodeConsole>  _input_sticky_keys_dispatcher;

		bool _key_status[Input::KEY_MAX + 1];

		void _handle_input();
		void _handle_mode_change();
		void _handle_cb_rom_change();
		void _handle_sticky_keys();

	public:

		GenodeConsole()
		:
			Console(),
			_gui(genode_env()),
			_input(*_gui.input()),
			_ax(0), _ay(0),
			_last_received_motion_event_was_absolute(false),
			_shape_report_connection(genode_env(), "shape",
			                         sizeof(Pointer::Shape_report)),
			_shape_report_ds(genode_env().rm(), _shape_report_connection.dataspace()),
			_shape_report(_shape_report_ds.local_addr<Pointer::Shape_report>()),
			_clipboard_reporter(nullptr),
			_clipboard_rom(nullptr),
			_vbox_keyboard(0),
			_vbox_mouse(0),
			_input_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_input),
			_mode_change_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_mode_change),
			_clipboard_signal_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_cb_rom_change),
			_input_sticky_keys_dispatcher(genode_env().ep(), *this, &GenodeConsole::handle_sticky_keys)
		{
			for (unsigned i = 0; i <= Input::KEY_MAX; i++)
				_key_status[i] = false;

			_input.sigh(_input_signal_dispatcher);

			Genode::Attached_rom_dataspace config(genode_env(), "config");

			/* by default we take the capslock key from the input stream */
			Genode::String<10> capslock("input");
			if (config.xml().attribute_value("capslock", capslock) == "ROM") {
				_caps_lock.construct(genode_env(), "capslock");
				_caps_lock->sigh(_input_sticky_keys_dispatcher);
			}
		}

		Gui::Connection &gui() { return _gui; }

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

			if (shape_size > Pointer::MAX_SHAPE_SIZE) {
				Genode::error(__func__, ": shape data buffer is too small for ",
				              shape_size, " bytes");
				return;
			}

			/* convert the shape data from BGRA encoding to RGBA encoding */

			unsigned char const *bgra_shape = shape; 
			unsigned char       *rgba_shape = _shape_report->shape;

			for (unsigned int y = 0; y < _shape_report->height; y++) {

				unsigned char const *bgra_line = &bgra_shape[y * _shape_report->width * 4];
				unsigned char *rgba_line       = &rgba_shape[y * _shape_report->width * 4];

				for (unsigned int i = 0; i < _shape_report->width * 4; i += 4) {
					rgba_line[i + 0] = bgra_line[i + 2];
					rgba_line[i + 1] = bgra_line[i + 1];
					rgba_line[i + 2] = bgra_line[i + 0];
					rgba_line[i + 3] = bgra_line[i + 3];
				}
			}

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

			_shape_report_connection.submit(sizeof(Pointer::Shape_report));
		}

		void update_video_mode();

		void handle_input();
		void handle_mode_change();
		void handle_cb_rom_change();
		void handle_sticky_keys();
};
