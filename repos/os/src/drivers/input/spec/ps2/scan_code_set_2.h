/*
 * \brief  Mapping from PS/2 Scan-Code Set 2 to unique key codes
 * \author Christian Helmuth
 * \date   2007-09-29
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCAN_CODE_SET_2_H_
#define _SCAN_CODE_SET_2_H_

#include <input/keycodes.h>

enum { SCAN_CODE_SET_2_NUM_KEYS = 256 };

static unsigned short scan_code_set_2[SCAN_CODE_SET_2_NUM_KEYS] = {
	/*  0 */ Input::KEY_UNKNOWN,
	/*  1 */ Input::KEY_F9,
	/*  2 */ Input::KEY_F7,
	/*  3 */ Input::KEY_F5,
	/*  4 */ Input::KEY_F3,
	/*  5 */ Input::KEY_F1,
	/*  6 */ Input::KEY_F2,
	/*  7 */ Input::KEY_F12,
	/*  8 */ Input::KEY_UNKNOWN,
	/*  9 */ Input::KEY_F10,
	/*  a */ Input::KEY_F8,
	/*  b */ Input::KEY_F6,
	/*  c */ Input::KEY_F4,
	/*  d */ Input::KEY_TAB,
	/*  e */ Input::KEY_GRAVE,
	/*  f */ Input::KEY_UNKNOWN,
	/* 10 */ Input::KEY_UNKNOWN,
	/* 11 */ Input::KEY_LEFTALT,
	/* 12 */ Input::KEY_LEFTSHIFT,
	/* 13 */ Input::KEY_UNKNOWN,
	/* 14 */ Input::KEY_LEFTCTRL,
	/* 15 */ Input::KEY_Q,
	/* 16 */ Input::KEY_1,
	/* 17 */ Input::KEY_UNKNOWN,
	/* 18 */ Input::KEY_UNKNOWN,
	/* 19 */ Input::KEY_UNKNOWN,
	/* 1a */ Input::KEY_Z,
	/* 1b */ Input::KEY_S,
	/* 1c */ Input::KEY_A,
	/* 1d */ Input::KEY_W,
	/* 1e */ Input::KEY_2,
	/* 1f */ Input::KEY_UNKNOWN,
	/* 20 */ Input::KEY_UNKNOWN,
	/* 21 */ Input::KEY_C,
	/* 22 */ Input::KEY_X,
	/* 23 */ Input::KEY_D,
	/* 24 */ Input::KEY_E,
	/* 25 */ Input::KEY_4,
	/* 26 */ Input::KEY_3,
	/* 27 */ Input::KEY_UNKNOWN,
	/* 28 */ Input::KEY_UNKNOWN,
	/* 29 */ Input::KEY_SPACE,
	/* 2a */ Input::KEY_V,
	/* 2b */ Input::KEY_F,
	/* 2c */ Input::KEY_T,
	/* 2d */ Input::KEY_R,
	/* 2e */ Input::KEY_5,
	/* 2f */ Input::KEY_UNKNOWN, /* F13? */
	/* 30 */ Input::KEY_UNKNOWN,
	/* 31 */ Input::KEY_N,
	/* 32 */ Input::KEY_B,
	/* 33 */ Input::KEY_H,
	/* 34 */ Input::KEY_G,
	/* 35 */ Input::KEY_Y,
	/* 36 */ Input::KEY_6,
	/* 37 */ Input::KEY_UNKNOWN, /* F14? */
	/* 38 */ Input::KEY_UNKNOWN,
	/* 39 */ Input::KEY_UNKNOWN,
	/* 3a */ Input::KEY_M,
	/* 3b */ Input::KEY_J,
	/* 3c */ Input::KEY_U,
	/* 3d */ Input::KEY_7,
	/* 3e */ Input::KEY_8,
	/* 3f */ Input::KEY_UNKNOWN,
	/* 40 */ Input::KEY_UNKNOWN,
	/* 41 */ Input::KEY_COMMA,
	/* 42 */ Input::KEY_K,
	/* 43 */ Input::KEY_I,
	/* 44 */ Input::KEY_O,
	/* 45 */ Input::KEY_0,
	/* 46 */ Input::KEY_9,
	/* 47 */ Input::KEY_UNKNOWN,
	/* 48 */ Input::KEY_UNKNOWN,
	/* 49 */ Input::KEY_DOT,
	/* 4a */ Input::KEY_SLASH,
	/* 4b */ Input::KEY_L,
	/* 4c */ Input::KEY_SEMICOLON,
	/* 4d */ Input::KEY_P,
	/* 4e */ Input::KEY_MINUS,
	/* 4f */ Input::KEY_UNKNOWN,
	/* 50 */ Input::KEY_UNKNOWN,
	/* 51 */ Input::KEY_UNKNOWN,
	/* 52 */ Input::KEY_APOSTROPHE,
	/* 53 */ Input::KEY_UNKNOWN,
	/* 54 */ Input::KEY_LEFTBRACE,
	/* 55 */ Input::KEY_EQUAL,
	/* 56 */ Input::KEY_UNKNOWN,
	/* 57 */ Input::KEY_UNKNOWN,
	/* 58 */ Input::KEY_CAPSLOCK,
	/* 59 */ Input::KEY_RIGHTSHIFT,
	/* 5a */ Input::KEY_ENTER,
	/* 5b */ Input::KEY_RIGHTBRACE,
	/* 5c */ Input::KEY_UNKNOWN,
	/* 5d */ Input::KEY_BACKSLASH,
	/* 5e */ Input::KEY_UNKNOWN,
	/* 5f */ Input::KEY_UNKNOWN,
	/* 60 */ Input::KEY_UNKNOWN,
	/* 61 */ Input::KEY_102ND,
	/* 62 */ Input::KEY_UNKNOWN,
	/* 63 */ Input::KEY_UNKNOWN,
	/* 64 */ Input::KEY_UNKNOWN,
	/* 65 */ Input::KEY_UNKNOWN,
	/* 66 */ Input::KEY_BACKSPACE,
	/* 67 */ Input::KEY_UNKNOWN,
	/* 68 */ Input::KEY_UNKNOWN,
	/* 69 */ Input::KEY_KP1,
	/* 6a */ Input::KEY_UNKNOWN,
	/* 6b */ Input::KEY_KP4,
	/* 6c */ Input::KEY_KP7,
	/* 6d */ Input::KEY_KPCOMMA,
	/* 6e */ Input::KEY_UNKNOWN,
	/* 6f */ Input::KEY_UNKNOWN,
	/* 70 */ Input::KEY_KP0,
	/* 71 */ Input::KEY_KPDOT,
	/* 72 */ Input::KEY_KP2,
	/* 73 */ Input::KEY_KP5,
	/* 74 */ Input::KEY_KP6,
	/* 75 */ Input::KEY_KP8,
	/* 76 */ Input::KEY_ESC,
	/* 77 */ Input::KEY_NUMLOCK,
	/* 78 */ Input::KEY_F11,
	/* 79 */ Input::KEY_KPPLUS,
	/* 7a */ Input::KEY_KP3,
	/* 7b */ Input::KEY_KPMINUS,
	/* 7c */ Input::KEY_KPASTERISK,
	/* 7d */ Input::KEY_KP9,
	/* 7e */ Input::KEY_SCROLLLOCK,
	/* 7f */ Input::KEY_SYSRQ,
	/* 80 */ Input::KEY_UNKNOWN,
	/* 81 */ Input::KEY_UNKNOWN,
	/* 82 */ Input::KEY_UNKNOWN,
	/* 83 */ Input::KEY_F7,
	/* 84 */ Input::KEY_SYSRQ,
	/* 85 */ Input::KEY_UNKNOWN,
	/* 86 */ Input::KEY_UNKNOWN,
	/* 87 */ Input::KEY_UNKNOWN,
	/* 88 */ Input::KEY_UNKNOWN,
	/* 89 */ Input::KEY_UNKNOWN,
	/* 8a */ Input::KEY_UNKNOWN,
	/* 8b */ Input::KEY_UNKNOWN,
	/* 8c */ Input::KEY_UNKNOWN,
	/* 8d */ Input::KEY_UNKNOWN,
	/* 8e */ Input::KEY_UNKNOWN,
	/* 8f */ Input::KEY_UNKNOWN,
	/* 90 */ Input::KEY_UNKNOWN,
	/* 91 */ Input::KEY_UNKNOWN,
	/* 92 */ Input::KEY_UNKNOWN,
	/* 93 */ Input::KEY_UNKNOWN,
	/* 94 */ Input::KEY_UNKNOWN,
	/* 95 */ Input::KEY_UNKNOWN,
	/* 96 */ Input::KEY_UNKNOWN,
	/* 97 */ Input::KEY_UNKNOWN,
	/* 98 */ Input::KEY_UNKNOWN,
	/* 99 */ Input::KEY_UNKNOWN,
	/* 9a */ Input::KEY_UNKNOWN,
	/* 9b */ Input::KEY_UNKNOWN,
	/* 9c */ Input::KEY_UNKNOWN,
	/* 9d */ Input::KEY_UNKNOWN,
	/* 9e */ Input::KEY_UNKNOWN,
	/* 9f */ Input::KEY_UNKNOWN,
	/* a0 */ Input::KEY_UNKNOWN,
	/* a1 */ Input::KEY_UNKNOWN,
	/* a2 */ Input::KEY_UNKNOWN,
	/* a3 */ Input::KEY_UNKNOWN,
	/* a4 */ Input::KEY_UNKNOWN,
	/* a5 */ Input::KEY_UNKNOWN,
	/* a6 */ Input::KEY_UNKNOWN,
	/* a7 */ Input::KEY_UNKNOWN,
	/* a8 */ Input::KEY_UNKNOWN,
	/* a9 */ Input::KEY_UNKNOWN,
	/* aa */ Input::KEY_UNKNOWN,
	/* ab */ Input::KEY_UNKNOWN,
	/* ac */ Input::KEY_UNKNOWN,
	/* ad */ Input::KEY_UNKNOWN,
	/* ae */ Input::KEY_UNKNOWN,
	/* af */ Input::KEY_UNKNOWN,
	/* b0 */ Input::KEY_UNKNOWN,
	/* b1 */ Input::KEY_UNKNOWN,
	/* b2 */ Input::KEY_UNKNOWN,
	/* b3 */ Input::KEY_UNKNOWN,
	/* b4 */ Input::KEY_UNKNOWN,
	/* b5 */ Input::KEY_UNKNOWN,
	/* b6 */ Input::KEY_UNKNOWN,
	/* b7 */ Input::KEY_UNKNOWN,
	/* b8 */ Input::KEY_UNKNOWN,
	/* b9 */ Input::KEY_UNKNOWN,
	/* ba */ Input::KEY_UNKNOWN,
	/* bb */ Input::KEY_UNKNOWN,
	/* bc */ Input::KEY_UNKNOWN,
	/* bd */ Input::KEY_UNKNOWN,
	/* be */ Input::KEY_UNKNOWN,
	/* bf */ Input::KEY_UNKNOWN,
	/* c0 */ Input::KEY_UNKNOWN,
	/* c1 */ Input::KEY_UNKNOWN,
	/* c2 */ Input::KEY_UNKNOWN,
	/* c3 */ Input::KEY_UNKNOWN,
	/* c4 */ Input::KEY_UNKNOWN,
	/* c5 */ Input::KEY_UNKNOWN,
	/* c6 */ Input::KEY_UNKNOWN,
	/* c7 */ Input::KEY_UNKNOWN,
	/* c8 */ Input::KEY_UNKNOWN,
	/* c9 */ Input::KEY_UNKNOWN,
	/* ca */ Input::KEY_UNKNOWN,
	/* cb */ Input::KEY_UNKNOWN,
	/* cc */ Input::KEY_UNKNOWN,
	/* cd */ Input::KEY_UNKNOWN,
	/* ce */ Input::KEY_UNKNOWN,
	/* cf */ Input::KEY_UNKNOWN,
	/* d0 */ Input::KEY_UNKNOWN,
	/* d1 */ Input::KEY_UNKNOWN,
	/* d2 */ Input::KEY_UNKNOWN,
	/* d3 */ Input::KEY_UNKNOWN,
	/* d4 */ Input::KEY_UNKNOWN,
	/* d5 */ Input::KEY_UNKNOWN,
	/* d6 */ Input::KEY_UNKNOWN,
	/* d7 */ Input::KEY_UNKNOWN,
	/* d8 */ Input::KEY_UNKNOWN,
	/* d9 */ Input::KEY_UNKNOWN,
	/* da */ Input::KEY_UNKNOWN,
	/* db */ Input::KEY_UNKNOWN,
	/* dc */ Input::KEY_UNKNOWN,
	/* dd */ Input::KEY_UNKNOWN,
	/* de */ Input::KEY_UNKNOWN,
	/* df */ Input::KEY_UNKNOWN,
	/* e0 */ Input::KEY_UNKNOWN,
	/* e1 */ Input::KEY_UNKNOWN,
	/* e2 */ Input::KEY_UNKNOWN,
	/* e3 */ Input::KEY_UNKNOWN,
	/* e4 */ Input::KEY_UNKNOWN,
	/* e5 */ Input::KEY_UNKNOWN,
	/* e6 */ Input::KEY_UNKNOWN,
	/* e7 */ Input::KEY_UNKNOWN,
	/* e8 */ Input::KEY_UNKNOWN,
	/* e9 */ Input::KEY_UNKNOWN,
	/* ea */ Input::KEY_UNKNOWN,
	/* eb */ Input::KEY_UNKNOWN,
	/* ec */ Input::KEY_UNKNOWN,
	/* ed */ Input::KEY_UNKNOWN,
	/* ee */ Input::KEY_UNKNOWN,
	/* ef */ Input::KEY_UNKNOWN,
	/* f0 */ Input::KEY_UNKNOWN,
	/* f1 */ Input::KEY_UNKNOWN,
	/* f2 */ Input::KEY_UNKNOWN,
	/* f3 */ Input::KEY_UNKNOWN,
	/* f4 */ Input::KEY_UNKNOWN,
	/* f5 */ Input::KEY_UNKNOWN,
	/* f6 */ Input::KEY_UNKNOWN,
	/* f7 */ Input::KEY_UNKNOWN,
	/* f8 */ Input::KEY_UNKNOWN,
	/* f9 */ Input::KEY_UNKNOWN,
	/* fa */ Input::KEY_UNKNOWN,
	/* fb */ Input::KEY_UNKNOWN,
	/* fc */ Input::KEY_UNKNOWN,
	/* fd */ Input::KEY_UNKNOWN,
	/* fe */ Input::KEY_UNKNOWN,
	/* ff */ Input::KEY_UNKNOWN,
};


/**
 * Scan-code table for keys with extended scan code
 */
static unsigned short scan_code_set_2_ext[SCAN_CODE_SET_2_NUM_KEYS];


/**
 * Init scan_code_set_2_ext table
 */
inline void init_scan_code_set_2_ext()
{
	for (int i = 0; i < SCAN_CODE_SET_2_NUM_KEYS; i++)
		scan_code_set_2_ext[i] = Input::KEY_UNKNOWN;

	scan_code_set_2_ext[0x11] = Input::KEY_RIGHTALT;
	scan_code_set_2_ext[0x12] = Input::KEY_RESERVED;  /* sometimes used as num-lock indicator */
	scan_code_set_2_ext[0x14] = Input::KEY_RIGHTCTRL;
	scan_code_set_2_ext[0x1f] = Input::KEY_LEFTMETA;
	scan_code_set_2_ext[0x27] = Input::KEY_RIGHTMETA;
	scan_code_set_2_ext[0x2f] = Input::KEY_COMPOSE;
	scan_code_set_2_ext[0x37] = Input::KEY_POWER;
	scan_code_set_2_ext[0x3f] = Input::KEY_SLEEP;
	scan_code_set_2_ext[0x4a] = Input::KEY_KPSLASH;
	scan_code_set_2_ext[0x5a] = Input::KEY_KPENTER;
	scan_code_set_2_ext[0x5e] = Input::KEY_WAKEUP;
	scan_code_set_2_ext[0x69] = Input::KEY_END;
	scan_code_set_2_ext[0x6b] = Input::KEY_LEFT;
	scan_code_set_2_ext[0x6c] = Input::KEY_HOME;
	scan_code_set_2_ext[0x70] = Input::KEY_INSERT;
	scan_code_set_2_ext[0x71] = Input::KEY_DELETE;
	scan_code_set_2_ext[0x72] = Input::KEY_DOWN;
	scan_code_set_2_ext[0x74] = Input::KEY_RIGHT;
	scan_code_set_2_ext[0x75] = Input::KEY_UP;
	scan_code_set_2_ext[0x77] = Input::KEY_PAUSE;     /* put here although it's prefixed with 0xe1 0x14 */
	scan_code_set_2_ext[0x7a] = Input::KEY_PAGEDOWN;
	scan_code_set_2_ext[0x7c] = Input::KEY_SYSRQ;
	scan_code_set_2_ext[0x7d] = Input::KEY_PAGEUP;
	scan_code_set_2_ext[0x7e] = Input::KEY_PAUSE;     /* emitted if CTRL is pressed too */
};

#endif /* _SCAN_CODE_SET_2_H_ */
