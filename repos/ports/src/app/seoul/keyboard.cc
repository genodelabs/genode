/*
 * \brief  Keyboard manager
 * \author Markus Partheymueller
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* local includes */
#include "keyboard.h"

/* vancouver generic keyboard helper */
#include <host/keyboard.h>
#include <nul/vcpu.h>


Seoul::Keyboard::Keyboard(Synced_motherboard &mb)
: _motherboard(mb), _flags(0) { }


bool Seoul::Keyboard::_map_keycode(unsigned &keycode, bool press)
{
	switch (keycode) {

	/* modifiers */
	case Input::KEY_LEFTSHIFT:  _flags |= KBFLAG_LSHIFT; keycode = 0x12; break;
	case Input::KEY_RIGHTSHIFT: _flags |= KBFLAG_RSHIFT; keycode = 0x59; break;
	case Input::KEY_LEFTALT:    _flags |= KBFLAG_LALT;   keycode = 0x11; break;
	case Input::KEY_RIGHTALT:   _flags |= KBFLAG_RALT;   keycode = 0x11; break;
	case Input::KEY_LEFTCTRL:   _flags |= KBFLAG_LCTRL;  keycode = 0x14; break;
	case Input::KEY_RIGHTCTRL:  _flags |= KBFLAG_RCTRL;  keycode = 0x14; break;
	case Input::KEY_LEFTMETA:   _flags |= KBFLAG_LWIN;   keycode = 0x1f;
		if (press) return false;
		break;
	case Input::KEY_RIGHTMETA:  _flags |= KBFLAG_RWIN;   keycode = 0x27;
		if (press) return false;
		break;
	case Input::KEY_KPSLASH:    _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x35); break;
	case Input::KEY_KPENTER:    _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x1c); break;
	case Input::KEY_F11:        _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x57); break;
	case Input::KEY_F12:        _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x58); break;
	case Input::KEY_INSERT:     _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x52); break;
	case Input::KEY_DELETE:     _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x53); break;
	case Input::KEY_HOME:       _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x47); break;
	case Input::KEY_END:        _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4f); break;
	case Input::KEY_PAGEUP:     _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x49); break;
	case Input::KEY_PAGEDOWN:   _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x51); break;
	case Input::KEY_LEFT:       _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4b); break;
	case Input::KEY_RIGHT:      _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x4d); break;
	case Input::KEY_UP:         _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x48); break;
	case Input::KEY_DOWN:       _flags |= KBFLAG_EXTEND0;
		keycode = GenericKeyboard::translate_sc1_to_sc2(0x50); break;

	/* up to 0x53, the Genode key codes correspond to scan code set 1 */
	default:
		if (keycode <= 0x53) {
			keycode = GenericKeyboard::translate_sc1_to_sc2(keycode);
			break;
		} else return false;
	}

	return true;
}


void Seoul::Keyboard::handle_keycode_press(unsigned keycode)
{
	unsigned orig_keycode = keycode;

	if (!_map_keycode(keycode, true))
		return;

	MessageInput msg(0x10000, _flags | keycode);

	/* debug */
	if ((_flags & KBFLAG_LWIN) && orig_keycode == Input::KEY_INSERT) {
		Logging::printf("DEBUG key\n");

		/* we send an empty event */
		CpuEvent msg(VCpu::EVENT_DEBUG);
		for (VCpu *vcpu = _motherboard()->last_vcpu; vcpu; vcpu=vcpu->get_last())
			vcpu->bus_event.send(msg);
	}

	/* reset */
	else if ((_flags & KBFLAG_LWIN) && orig_keycode == Input::KEY_END) {
		Logging::printf("Reset VM\n");
		MessageLegacy msg2(MessageLegacy::RESET, 0);
		_motherboard()->bus_legacy.send_fifo(msg2);
	}

	else _motherboard()->bus_input.send(msg);

	_flags &= ~(KBFLAG_EXTEND0 | KBFLAG_RELEASE | KBFLAG_EXTEND1);
}


void Seoul::Keyboard::handle_keycode_release(unsigned keycode)
{
	_flags |= KBFLAG_RELEASE;

	if (!_map_keycode(keycode, false))
		return;

	MessageInput msg(0x10000, _flags | keycode);
	_motherboard()->bus_input.send(msg);

	_flags &= ~(KBFLAG_EXTEND0 | KBFLAG_RELEASE | KBFLAG_EXTEND1);
}
