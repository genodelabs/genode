/*
 * \brief  Keyboard manager
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 *
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#include <vancouver_keyboard.h>

/* Vancouver GenericKeyboard helper */
#include <host/keyboard.h>
#include <nul/vcpu.h>

extern Genode::Lock global_lock;

Vancouver_keyboard::Vancouver_keyboard(Motherboard &mb)
: _mb(mb), _flags(0) { }

void Vancouver_keyboard::handle_keycode_press(unsigned keycode)
{
	unsigned orig_keycode = keycode;

	switch (keycode) {
	/* Modifiers */
	case Input::KEY_LEFTSHIFT:	_flags |= KBFLAG_LSHIFT; keycode = 0x12; break;
	case Input::KEY_RIGHTSHIFT:	_flags |= KBFLAG_RSHIFT; keycode = 0x59; break;
	case Input::KEY_LEFTALT:  	_flags |= KBFLAG_LALT; keycode = 0x11; break;
	case Input::KEY_RIGHTALT:  	_flags |= KBFLAG_RALT; keycode = 0x11; break;
	case Input::KEY_LEFTCTRL: 	_flags |= KBFLAG_LCTRL; keycode = 0x14; break;
	case Input::KEY_RIGHTCTRL: 	_flags |= KBFLAG_RCTRL; keycode = 0x14; break;
	case Input::KEY_LEFTMETA:	_flags |= KBFLAG_LWIN; keycode = 0x1f; return;
	case Input::KEY_RIGHTMETA:	_flags |= KBFLAG_RWIN; keycode = 0x27; return;

	case Input::KEY_KPSLASH: 	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x35); break;
	case Input::KEY_KPENTER:	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x1c); break;
	case Input::KEY_F11:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x57); break;
	case Input::KEY_F12:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x58); break;
	case Input::KEY_INSERT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x52); break;
	case Input::KEY_DELETE:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x53); break;
	case Input::KEY_HOME:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x47); break;
	case Input::KEY_END:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4f); break;
	case Input::KEY_PAGEUP:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x49); break;
	case Input::KEY_PAGEDOWN:	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x51); break;
	case Input::KEY_LEFT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4b); break;
	case Input::KEY_RIGHT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4d); break;
	case Input::KEY_UP:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x48); break;
	case Input::KEY_DOWN:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x50); break;

	/* Up to 0x53, the Genode Keycodes correspond to scanset1. */
	default:	 	if (keycode <= 0x53) {
			keycode = GenericKeyboard::translate_sc1_to_sc2(keycode);
			break;
		} else return;
	}

	MessageInput msg(0x10000, _flags | keycode);

	/* Debug */
	if ((_flags & KBFLAG_LWIN) && orig_keycode == Input::KEY_INSERT) {
		Logging::printf("DEBUG key\n");

		/* we send an empty event */
		CpuEvent msg(VCpu::EVENT_DEBUG);
		for (VCpu *vcpu = _mb.last_vcpu; vcpu; vcpu=vcpu->get_last())
			vcpu->bus_event.send(msg);
	}

	/* Reset */
	else if ((_flags & KBFLAG_LWIN) && orig_keycode == Input::KEY_END) {
		Genode::Lock::Guard guard(global_lock);
		Logging::printf("Reset VM\n");
		MessageLegacy msg2(MessageLegacy::RESET, 0);
		_mb.bus_legacy.send_fifo(msg2);
	}

	else _mb.bus_input.send(msg);

	_flags &= ~(KBFLAG_EXTEND0 | KBFLAG_RELEASE | KBFLAG_EXTEND1);
}

void Vancouver_keyboard::handle_keycode_release(unsigned keycode)
{
	_flags |= KBFLAG_RELEASE;

	switch (keycode) {

	/* Modifiers */
	case Input::KEY_LEFTSHIFT:	_flags |= KBFLAG_LSHIFT; keycode = 0x12; break;
	case Input::KEY_RIGHTSHIFT:	_flags |= KBFLAG_RSHIFT; keycode = 0x59; break;
	case Input::KEY_LEFTALT:	_flags |= KBFLAG_LALT; keycode = 0x11; break;
	case Input::KEY_RIGHTALT:	_flags |= KBFLAG_RALT; keycode = 0x11; break;
	case Input::KEY_LEFTCTRL: 	_flags |= KBFLAG_LCTRL; keycode = 0x14; break;
	case Input::KEY_RIGHTCTRL: 	_flags |= KBFLAG_RCTRL; keycode = 0x14; break;
	case Input::KEY_LEFTMETA:	_flags |= KBFLAG_LWIN; keycode = 0x1f; break;
	case Input::KEY_RIGHTMETA:	_flags |= KBFLAG_RWIN; keycode = 0x27; break;

	case Input::KEY_KPSLASH: 	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x35); break;
	case Input::KEY_KPENTER:	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x1c); break;
	case Input::KEY_F11:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x57); break;
	case Input::KEY_F12:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x58); break;
	case Input::KEY_INSERT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x52); break;
	case Input::KEY_DELETE:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x53); break;
	case Input::KEY_HOME:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x47); break;
	case Input::KEY_END:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4f); break;
	case Input::KEY_PAGEUP:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x49); break;
	case Input::KEY_PAGEDOWN:	_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x51); break;
	case Input::KEY_LEFT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4b); break;
	case Input::KEY_RIGHT:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4d); break;
	case Input::KEY_UP:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x48); break;
	case Input::KEY_DOWN:		_flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x50); break;

	/* Up to 0x53, the Genode Keycodes correspond to scanset1. */
	default:	 	if (keycode <= 0x53) {
			keycode = GenericKeyboard::translate_sc1_to_sc2(keycode);
			break;
		} else return;
	}

	MessageInput msg(0x10000, _flags | keycode);
	_mb.bus_input.send(msg);

	_flags &= ~(KBFLAG_EXTEND0 | KBFLAG_RELEASE | KBFLAG_EXTEND1);
}
