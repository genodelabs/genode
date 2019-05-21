/*
 * \brief  Zircon pc-ps2 driver
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <io_port_session/connection.h>
#include <timer_session/connection.h>
#include <platform_session/connection.h>
#include <input/component.h>
#include <input/root.h>

#include <ddk/driver.h>
#include <ddk/protocol/hidbus.h>
#include <zircon/device/input.h>

#include <zx/static_resource.h>
#include <zx/device.h>
#include <zx/driver.h>

#include <keymap.h>

struct Main
{
	static Genode::uint8_t mouse_btn;
	static Genode::uint8_t kbd_button[8];

	void mouse_button_event(int old_st, int new_st, int key)
	{
		int old_state = old_st & key;
		int new_state = new_st & key;
		int key_code = Input::KEY_UNKNOWN;

		if (old_state == new_state){
			return;
		}

		switch (key){
		case 1: key_code = Input::BTN_LEFT;
			break;
		case 2: key_code = Input::BTN_RIGHT;
			break;
		case 4: key_code = Input::BTN_MIDDLE;
			break;
		default: Genode::log("unsupported mouse button: ", key);
		}

		if (new_state){
			_ev_queue.add(Input::Press{Input::Keycode(key_code)});
		}else{
			_ev_queue.add(Input::Release{Input::Keycode(key_code)});
		}
	}

	bool contains(Genode::uint8_t const *buf, Genode::uint8_t const val)
	{
		for (int i = 2; i < 8; i++){
			if (buf[i] == val){
				return true;
			}
		}
		return false;
	}

	void handle_modifier(Genode::uint8_t current, int key)
	{
		int c_mod = current & key;
		int o_mod = kbd_button[0] & key;
		int key_code;

		if (c_mod == o_mod){
			return;
		}

		switch (key){
		case 1:
			key_code = Input::KEY_LEFTCTRL;
			break;
		case 2:
			key_code = Input::KEY_LEFTSHIFT;
			break;
		case 4:
			key_code = Input::KEY_LEFTALT;
			break;
		case 8:
			key_code = Input::KEY_LEFTMETA;
			break;
		case 16:
			key_code = Input::KEY_RIGHTCTRL;
			break;
		case 64:
			key_code = Input::KEY_RIGHTALT;
			break;
		default:
			Genode::log("unsupported modifier: ", key, " ", current);
			return;
		}

		if (c_mod){
			_ev_queue.add(Input::Press{Input::Keycode(key_code)});
		}else{
			_ev_queue.add(Input::Release{Input::Keycode(key_code)});
		}

	}

	void handle_keyboard(Genode::uint8_t const *current)
	{
		const Genode::uint8_t rollover[6] = {1, 1, 1, 1, 1, 1};
		if (!Genode::memcmp(&current[2], rollover, 6)){
			return;
		}

		for (int i = 2; i < 8; i++){
			if (current[i] != 0){
				if (!contains(kbd_button, current[i])){
					_ev_queue.add(Input::Press{Input::Keycode(zxg_keymap[current[i]])});
				}
			}
			if (kbd_button[i] != 0){
				if (!contains(current, kbd_button[i])){
					_ev_queue.add(Input::Release{Input::Keycode(zxg_keymap[kbd_button[i]])});
				}
			}
		}

		handle_modifier(current[0], 1);
		handle_modifier(current[0], 2);
		handle_modifier(current[0], 4);
		handle_modifier(current[0], 8);
		handle_modifier(current[0], 16);
		handle_modifier(current[0], 64);

		Genode::memcpy(kbd_button, current, sizeof(kbd_button));
	}

	void handle_mouse(boot_mouse_report_t const *report)
	{
		if (report->rel_x || report->rel_y){
			_ev_queue.add(Input::Relative_motion{report->rel_x, report->rel_y});
		}
		mouse_button_event(mouse_btn, report->buttons, 1);
		mouse_button_event(mouse_btn, report->buttons, 2);
		mouse_button_event(mouse_btn, report->buttons, 4);
		mouse_btn = report->buttons;
	}

	static void io_queue(void *cookie, const uint8_t *data, size_t size)
	{
		switch (size){
		case sizeof(boot_kbd_report_t):
			static_cast<Main *>(cookie)->handle_keyboard(data);
			break;
		case sizeof(boot_mouse_report_t):
			static_cast<Main *>(cookie)->handle_mouse(reinterpret_cast<boot_mouse_report_t const *>(data));
			break;
		default:
			Genode::warning("invalid data received");
			break;
		}
	}

	Genode::Env &_env;
	Genode::Heap _heap;
	Timer::Connection _timer;
	Platform::Connection _platform;
	Platform::Device_client _ps2_dev;

	ZX::Platform::Io_port _io_config[2] = { {0x60, 0x0}, {0x64, 0x1} };
	ZX::Platform::Irq _irq_config[2] = { {0x1, 0x0}, {0xc, 0x1} };
	hidbus_ifc_t _hidbus = { Main::io_queue };
	ZX::Device _zx_dev;

	Input::Session_component _session;
	Input::Root_component _root;
	Input::Event_queue &_ev_queue;

	Main(Genode::Env &env) :
		_env(env),
		_heap(env.ram(), env.rm()),
		_timer(env),
		_platform(env),
		_ps2_dev(_platform.with_upgrade([&] () {
			                                return _platform.device("PS2"); })),
		_zx_dev(this, true),
		_session(_env, _env.ram()),
		_root(_env.ep().rpc_ep(), _session),
		_ev_queue(_session.event_queue())
	{
		Genode::log("zircon pc-ps2 driver");
		_zx_dev.set_io_port(_io_config, 2);
		_zx_dev.set_irq(_irq_config, 2);
		_zx_dev.set_hidbus(&_hidbus);
		ZX::Resource<Genode::Env>::set_component(_env);
		ZX::Resource<Genode::Heap>::set_component(_heap);
		ZX::Resource<Timer::Connection>::set_component(_timer);
		ZX::Resource<Platform::Device_client>::set_component(_ps2_dev);
		ZX::Resource<ZX::Device>::set_component(_zx_dev);
		bind_driver(nullptr, nullptr);
		_env.parent().announce(_env.ep().manage(_root));
	}
};

Genode::uint8_t Main::mouse_btn = 0;
Genode::uint8_t Main::kbd_button[8] = {};

void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();
	static Main main(env);
}
