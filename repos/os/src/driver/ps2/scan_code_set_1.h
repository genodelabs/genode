/*
 * \brief  Mapping from PS/2 Scan-Code Set 1 to unique key codes
 * \author Norman Feske
 * \date   2007-09-29
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCAN_CODE_SET_1_H_
#define _SCAN_CODE_SET_1_H_

#include <input/keycodes.h>

enum { SCAN_CODE_SET_1_NUM_KEYS = 128 };

static unsigned short scan_code_set_1[SCAN_CODE_SET_1_NUM_KEYS] = {
	/*  0 */ Input::KEY_RESERVED,
	/*  1 */ Input::KEY_ESC,
	/*  2 */ Input::KEY_1,
	/*  3 */ Input::KEY_2,
	/*  4 */ Input::KEY_3,
	/*  5 */ Input::KEY_4,
	/*  6 */ Input::KEY_5,
	/*  7 */ Input::KEY_6,
	/*  8 */ Input::KEY_7,
	/*  9 */ Input::KEY_8,
	/*  a */ Input::KEY_9,
	/*  b */ Input::KEY_0,
	/*  c */ Input::KEY_MINUS,
	/*  d */ Input::KEY_EQUAL,
	/*  e */ Input::KEY_BACKSPACE,
	/*  f */ Input::KEY_TAB,
	/* 10 */ Input::KEY_Q,
	/* 11 */ Input::KEY_W,
	/* 12 */ Input::KEY_E,
	/* 13 */ Input::KEY_R,
	/* 14 */ Input::KEY_T,
	/* 15 */ Input::KEY_Y,
	/* 16 */ Input::KEY_U,
	/* 17 */ Input::KEY_I,
	/* 18 */ Input::KEY_O,
	/* 19 */ Input::KEY_P,
	/* 1a */ Input::KEY_LEFTBRACE,
	/* 1b */ Input::KEY_RIGHTBRACE,
	/* 1c */ Input::KEY_ENTER,
	/* 1d */ Input::KEY_LEFTCTRL,
	/* 1e */ Input::KEY_A,
	/* 1f */ Input::KEY_S,
	/* 20 */ Input::KEY_D,
	/* 21 */ Input::KEY_F,
	/* 22 */ Input::KEY_G,
	/* 23 */ Input::KEY_H,
	/* 24 */ Input::KEY_J,
	/* 25 */ Input::KEY_K,
	/* 26 */ Input::KEY_L,
	/* 27 */ Input::KEY_SEMICOLON,
	/* 28 */ Input::KEY_APOSTROPHE,
	/* 29 */ Input::KEY_GRAVE,
	/* 2a */ Input::KEY_LEFTSHIFT,
	/* 2b */ Input::KEY_BACKSLASH,
	/* 2c */ Input::KEY_Z,
	/* 2d */ Input::KEY_X,
	/* 2e */ Input::KEY_C,
	/* 2f */ Input::KEY_V,
	/* 30 */ Input::KEY_B,
	/* 31 */ Input::KEY_N,
	/* 32 */ Input::KEY_M,
	/* 33 */ Input::KEY_COMMA,
	/* 34 */ Input::KEY_DOT,
	/* 35 */ Input::KEY_SLASH,
	/* 36 */ Input::KEY_RIGHTSHIFT,
	/* 37 */ Input::KEY_KPASTERISK,
	/* 38 */ Input::KEY_LEFTALT,
	/* 39 */ Input::KEY_SPACE,
	/* 3a */ Input::KEY_CAPSLOCK,
	/* 3b */ Input::KEY_F1,
	/* 3c */ Input::KEY_F2,
	/* 3d */ Input::KEY_F3,
	/* 3e */ Input::KEY_F4,
	/* 3f */ Input::KEY_F5,
	/* 40 */ Input::KEY_F6,
	/* 41 */ Input::KEY_F7,
	/* 42 */ Input::KEY_F8,
	/* 43 */ Input::KEY_F9,
	/* 44 */ Input::KEY_F10,
	/* 45 */ Input::KEY_NUMLOCK,
	/* 46 */ Input::KEY_SCROLLLOCK,
	/* 47 */ Input::KEY_KP7,
	/* 48 */ Input::KEY_KP8,
	/* 49 */ Input::KEY_KP9,
	/* 4a */ Input::KEY_KPMINUS,
	/* 4b */ Input::KEY_KP4,
	/* 4c */ Input::KEY_KP5,
	/* 4d */ Input::KEY_KP6,
	/* 4e */ Input::KEY_KPPLUS,
	/* 4f */ Input::KEY_KP1,
	/* 50 */ Input::KEY_KP2,
	/* 51 */ Input::KEY_KP3,
	/* 52 */ Input::KEY_KP0,
	/* 53 */ Input::KEY_KPDOT,
	/* 54 */ Input::KEY_SYSRQ,
	/* 55 */ Input::KEY_UNKNOWN,
	/* 56 */ Input::KEY_102ND,
	/* 57 */ Input::KEY_F11,
	/* 58 */ Input::KEY_F12,
	/* 59 */ Input::KEY_UNKNOWN,
	/* 5a */ Input::KEY_UNKNOWN,
	/* 5b */ Input::KEY_UNKNOWN,
	/* 5c */ Input::KEY_UNKNOWN,
	/* 5d */ Input::KEY_UNKNOWN,
	/* 5e */ Input::KEY_UNKNOWN,
	/* 5f */ Input::KEY_UNKNOWN,
	/* 60 */ Input::KEY_UNKNOWN,
	/* 61 */ Input::KEY_UNKNOWN,
	/* 62 */ Input::KEY_UNKNOWN,
	/* 63 */ Input::KEY_UNKNOWN,
	/* 64 */ Input::KEY_UNKNOWN,
	/* 65 */ Input::KEY_UNKNOWN,
	/* 66 */ Input::KEY_UNKNOWN,
	/* 67 */ Input::KEY_UNKNOWN,
	/* 68 */ Input::KEY_UNKNOWN,
	/* 69 */ Input::KEY_UNKNOWN,
	/* 6a */ Input::KEY_UNKNOWN,
	/* 6b */ Input::KEY_UNKNOWN,
	/* 6c */ Input::KEY_UNKNOWN,
	/* 6d */ Input::KEY_UNKNOWN,
	/* 6e */ Input::KEY_UNKNOWN,
	/* 6f */ Input::KEY_SCREENLOCK,
	/* 70 */ Input::KEY_UNKNOWN,
	/* 71 */ Input::KEY_UNKNOWN,
	/* 72 */ Input::KEY_UNKNOWN,
	/* 73 */ Input::KEY_UNKNOWN,
	/* 74 */ Input::KEY_UNKNOWN,
	/* 75 */ Input::KEY_UNKNOWN,
	/* 76 */ Input::KEY_UNKNOWN,
	/* 77 */ Input::KEY_UNKNOWN,
	/* 78 */ Input::KEY_UNKNOWN,
	/* 79 */ Input::KEY_UNKNOWN,
	/* 7a */ Input::KEY_UNKNOWN,
	/* 7b */ Input::KEY_UNKNOWN,
	/* 7c */ Input::KEY_UNKNOWN,
	/* 7d */ Input::KEY_UNKNOWN,
	/* 7e */ Input::KEY_UNKNOWN,
	/* 7f */ Input::KEY_UNKNOWN,
};


/**
 * Scan-code table for keys with a prefix of 0xe0
 */
static unsigned short scan_code_set_1_0xe0[SCAN_CODE_SET_1_NUM_KEYS];


/**
 * Init scan_code_set_1_0xe0 table
 */
inline void init_scan_code_set_1_0xe0()
{
	for (int i = 0; i < SCAN_CODE_SET_1_NUM_KEYS; i++)
		scan_code_set_1_0xe0[i] = Input::KEY_UNKNOWN;

	scan_code_set_1_0xe0[0x1c] = Input::KEY_KPENTER;
	scan_code_set_1_0xe0[0x1d] = Input::KEY_RIGHTCTRL;
	scan_code_set_1_0xe0[0x20] = Input::KEY_MUTE;
	scan_code_set_1_0xe0[0x2e] = Input::KEY_VOLUMEDOWN;
	scan_code_set_1_0xe0[0x30] = Input::KEY_VOLUMEUP;
	scan_code_set_1_0xe0[0x35] = Input::KEY_KPSLASH;
	scan_code_set_1_0xe0[0x37] = Input::KEY_PRINT;
	scan_code_set_1_0xe0[0x38] = Input::KEY_RIGHTALT;
	scan_code_set_1_0xe0[0x46] = Input::KEY_PAUSE;     /* emitted if CTRL is pressed too */
	scan_code_set_1_0xe0[0x47] = Input::KEY_HOME;
	scan_code_set_1_0xe0[0x48] = Input::KEY_UP;
	scan_code_set_1_0xe0[0x49] = Input::KEY_PAGEUP;
	scan_code_set_1_0xe0[0x4b] = Input::KEY_LEFT;
	scan_code_set_1_0xe0[0x4d] = Input::KEY_RIGHT;
	scan_code_set_1_0xe0[0x4f] = Input::KEY_END;
	scan_code_set_1_0xe0[0x50] = Input::KEY_DOWN;
	scan_code_set_1_0xe0[0x51] = Input::KEY_PAGEDOWN;
	scan_code_set_1_0xe0[0x52] = Input::KEY_INSERT;
	scan_code_set_1_0xe0[0x53] = Input::KEY_DELETE;
	scan_code_set_1_0xe0[0x5b] = Input::KEY_LEFTMETA;
	scan_code_set_1_0xe0[0x5c] = Input::KEY_RIGHTMETA;
	scan_code_set_1_0xe0[0x5d] = Input::KEY_COMPOSE;
	scan_code_set_1_0xe0[0x5e] = Input::KEY_POWER;
	scan_code_set_1_0xe0[0x5f] = Input::KEY_SLEEP;
	scan_code_set_1_0xe0[0x63] = Input::KEY_WAKEUP;
}

#endif /* _SCAN_CODE_SET_1_H_ */
