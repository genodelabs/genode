/*
 * \brief  Genode-specific event backend
 * \author Stefan Kalkowski
 * \date   2008-12-12
 */

/*
 * Copyright (c) <2008> Stefan Kalkowski
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* Genode includes */
#include <base/log.h>
#include <input_session/connection.h>
#include <input/event.h>
#include <input/keycodes.h>

/* local includes */
#include <SDL_genode_internal.h>


Genode::Lock event_lock;
Video video_events;


static Genode::Env *_global_env = nullptr;


Genode::Env *global_env()
{
	if (!_global_env) {
		Genode::error("sdl_init_genode() not called, aborting");
		throw Genode::Exception();
	}

	return _global_env;
}


void sdl_init_genode(Genode::Env &env)
{
	_global_env = &env;
}


extern "C" {

#include <SDL/SDL.h>
#include "SDL_events_c.h"
#include "SDL_sysevents.h"
#include "SDL_genode_fb_events.h"


	static Genode::Constructible<Input::Connection> input;
	static const int KEYNUM_MAX = 512;
	static SDLKey keymap[KEYNUM_MAX];
	static int buttonmap[KEYNUM_MAX];


	inline SDL_keysym *Genode_Fb_TranslateKey(int keycode, SDL_keysym *k)
	{
		k->scancode = keycode;
		k->sym = keymap[keycode];
		k->mod = SDL_GetModState();
		k->unicode = keymap[keycode];
		return k;
	}


	void Genode_Fb_PumpEvents(SDL_VideoDevice *t)
	{
		Genode::Lock_guard<Genode::Lock> guard(event_lock);

		if (video_events.resize_pending) {
			video_events.resize_pending = false;

			SDL_PrivateResize(video_events.width, video_events.height);
		}

		if (!input->pending())
			return;

		input->for_each_event([&] (Input::Event const &curr) {
			SDL_keysym ksym;
			switch(curr.type())
			{
			case Input::Event::MOTION:
				if (curr.absolute_motion())
					SDL_PrivateMouseMotion(0, 0, curr.ax(), curr.ay());
				else
					SDL_PrivateMouseMotion(0, 1, curr.rx(), curr.ry());
				break;
			case Input::Event::PRESS:
				if(curr.code() >= Input::BTN_MISC &&
				   curr.code() <= Input::BTN_GEAR_UP)
					SDL_PrivateMouseButton(SDL_PRESSED,
					                       buttonmap[curr.code()],
					                       0, 0);
				else
					SDL_PrivateKeyboard(SDL_PRESSED,
					                    Genode_Fb_TranslateKey(curr.code(),
					                    &ksym));
				break;
			case Input::Event::RELEASE:
				if(curr.code() >= Input::BTN_MISC &&
				   curr.code() <= Input::BTN_GEAR_UP)
					SDL_PrivateMouseButton(SDL_RELEASED,
					                       buttonmap[curr.code()],
					                       0, 0);
				else
					SDL_PrivateKeyboard(SDL_RELEASED,
					                    Genode_Fb_TranslateKey(curr.code(),
					                    &ksym));
				break;
			case Input::Event::WHEEL:
				Genode::warning("mouse wheel, not implemented yet");
				break;
			default:
				break;
			}
		});
	}


	void Genode_Fb_InitOSKeymap(SDL_VideoDevice *t)
	{
		try {
			input.construct(*_global_env);
		} catch (...) {
			Genode::error("no input driver available!");
			return;
		}

		using namespace Input;

		/* Prepare button mappings */
		for (int i=0; i<KEYNUM_MAX; i++)
		{
			switch(i)
			{
			case BTN_LEFT: buttonmap[i]=SDL_BUTTON_LEFT; break;
			case BTN_RIGHT: buttonmap[i]=SDL_BUTTON_RIGHT; break;
			case BTN_MIDDLE: buttonmap[i]=SDL_BUTTON_MIDDLE; break;
			case BTN_0:
			case BTN_1:
			case BTN_2:
			case BTN_3:
			case BTN_4:
			case BTN_5:
			case BTN_6:
			case BTN_7:
			case BTN_8:
			case BTN_9:
			case BTN_SIDE:
			case BTN_EXTRA:
			case BTN_FORWARD:
			case BTN_BACK:
			case BTN_TASK:
			case BTN_TRIGGER:
			case BTN_THUMB:
			case BTN_THUMB2:
			case BTN_TOP:
			case BTN_TOP2:
			case BTN_PINKIE:
			case BTN_BASE:
			case BTN_BASE2:
			case BTN_BASE3:
			case BTN_BASE4:
			case BTN_BASE5:
			case BTN_BASE6:
			case BTN_DEAD:
			case BTN_A:
			case BTN_B:
			case BTN_C:
			case BTN_X:
			case BTN_Y:
			case BTN_Z:
			case BTN_TL:
			case BTN_TR:
			case BTN_TL2:
			case BTN_TR2:
			case BTN_SELECT:
			case BTN_START:
			case BTN_MODE:
			case BTN_THUMBL:
			case BTN_THUMBR:
			case BTN_TOOL_PEN:
			case BTN_TOOL_RUBBER:
			case BTN_TOOL_BRUSH:
			case BTN_TOOL_PENCIL:
			case BTN_TOOL_AIRBRUSH:
			case BTN_TOOL_FINGER:
			case BTN_TOOL_MOUSE:
			case BTN_TOOL_LENS:
			case BTN_TOUCH:
			case BTN_STYLUS:
			case BTN_STYLUS2:
			case BTN_TOOL_DOUBLETAP:
			case BTN_TOOL_TRIPLETAP:
			case BTN_GEAR_DOWN:
			case BTN_GEAR_UP:
			default: buttonmap[i]=0;
			}
		}

		/* Prepare key mappings */
		for(int i=0; i<KEYNUM_MAX; i++)
		{
			switch(i)
			{
			case KEY_RESERVED: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ESC: keymap[i]=SDLK_ESCAPE; break;
			case KEY_1: keymap[i]=SDLK_1; break;
			case KEY_2: keymap[i]=SDLK_2; break;
			case KEY_3: keymap[i]=SDLK_3; break;
			case KEY_4: keymap[i]=SDLK_4; break;
			case KEY_5: keymap[i]=SDLK_5; break;
			case KEY_6: keymap[i]=SDLK_6; break;
			case KEY_7: keymap[i]=SDLK_7; break;
			case KEY_8: keymap[i]=SDLK_8; break;
			case KEY_9: keymap[i]=SDLK_9; break;
			case KEY_0: keymap[i]=SDLK_0; break;
			case KEY_MINUS: keymap[i]=SDLK_MINUS; break;
			case KEY_EQUAL: keymap[i]=SDLK_EQUALS; break;
			case KEY_BACKSPACE: keymap[i]=SDLK_BACKSPACE; break;
			case KEY_TAB: keymap[i]=SDLK_TAB; break;
			case KEY_Q: keymap[i]=SDLK_q; break;
			case KEY_W: keymap[i]=SDLK_w; break;
			case KEY_E: keymap[i]=SDLK_e; break;
			case KEY_R: keymap[i]=SDLK_r; break;
			case KEY_T: keymap[i]=SDLK_t; break;
			case KEY_Y: keymap[i]=SDLK_y; break;
			case KEY_U: keymap[i]=SDLK_u; break;
			case KEY_I: keymap[i]=SDLK_i; break;
			case KEY_O: keymap[i]=SDLK_o; break;
			case KEY_P: keymap[i]=SDLK_p; break;
			case KEY_LEFTBRACE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RIGHTBRACE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ENTER: keymap[i]=SDLK_RETURN; break;
			case KEY_LEFTCTRL: keymap[i]=SDLK_LCTRL; break;
			case KEY_A: keymap[i]=SDLK_a; break;
			case KEY_S: keymap[i]=SDLK_s; break;
			case KEY_D: keymap[i]=SDLK_d; break;
			case KEY_F: keymap[i]=SDLK_f; break;
			case KEY_G: keymap[i]=SDLK_g; break;
			case KEY_H: keymap[i]=SDLK_h; break;
			case KEY_J: keymap[i]=SDLK_j; break;
			case KEY_K: keymap[i]=SDLK_k; break;
			case KEY_L: keymap[i]=SDLK_l; break;
			case KEY_SEMICOLON: keymap[i]=SDLK_SEMICOLON; break;
			case KEY_APOSTROPHE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_GRAVE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_LEFTSHIFT: keymap[i]=SDLK_LSHIFT; break;
			case KEY_BACKSLASH: keymap[i]=SDLK_BACKSLASH; break;
			case KEY_Z: keymap[i]=SDLK_z; break;
			case KEY_X: keymap[i]=SDLK_x; break;
			case KEY_C: keymap[i]=SDLK_c; break;
			case KEY_V: keymap[i]=SDLK_v; break;
			case KEY_B: keymap[i]=SDLK_b; break;
			case KEY_N: keymap[i]=SDLK_n; break;
			case KEY_M: keymap[i]=SDLK_m; break;
			case KEY_COMMA: keymap[i]=SDLK_COMMA; break;
			case KEY_DOT: keymap[i]=SDLK_PERIOD; break;
			case KEY_SLASH: keymap[i]=SDLK_SLASH; break;
			case KEY_RIGHTSHIFT: keymap[i]=SDLK_RSHIFT; break;
			case KEY_KPASTERISK: keymap[i]=SDLK_ASTERISK; break;
			case KEY_LEFTALT: keymap[i]=SDLK_LALT; break;
			case KEY_SPACE: keymap[i]=SDLK_SPACE; break;
			case KEY_CAPSLOCK: keymap[i]=SDLK_CAPSLOCK; break;
			case KEY_F1: keymap[i]=SDLK_F1; break;
			case KEY_F2: keymap[i]=SDLK_F2; break;
			case KEY_F3: keymap[i]=SDLK_F3; break;
			case KEY_F4: keymap[i]=SDLK_F4; break;
			case KEY_F5: keymap[i]=SDLK_F5; break;
			case KEY_F6: keymap[i]=SDLK_F6; break;
			case KEY_F7: keymap[i]=SDLK_F7; break;
			case KEY_F8: keymap[i]=SDLK_F8; break;
			case KEY_F9: keymap[i]=SDLK_F9; break;
			case KEY_F10: keymap[i]=SDLK_F10; break;
			case KEY_NUMLOCK: keymap[i]=SDLK_NUMLOCK; break;
			case KEY_SCROLLLOCK: keymap[i]=SDLK_SCROLLOCK; break;
			case KEY_KP7: keymap[i]=SDLK_KP7; break;
			case KEY_KP8: keymap[i]=SDLK_KP8; break;
			case KEY_KP9: keymap[i]=SDLK_KP9; break;
			case KEY_KPMINUS: keymap[i]=SDLK_KP_MINUS; break;
			case KEY_KP4: keymap[i]=SDLK_KP4; break;
			case KEY_KP5: keymap[i]=SDLK_KP5; break;
			case KEY_KP6: keymap[i]=SDLK_KP6; break;
			case KEY_KPPLUS: keymap[i]=SDLK_KP_PLUS; break;
			case KEY_KP1: keymap[i]=SDLK_KP1; break;
			case KEY_KP2: keymap[i]=SDLK_KP2; break;
			case KEY_KP3: keymap[i]=SDLK_KP3; break;
			case KEY_KP0: keymap[i]=SDLK_KP0; break;
			case KEY_KPDOT: keymap[i]=SDLK_KP_PERIOD; break;
			case KEY_ZENKAKUHANKAKU: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_102ND: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F11: keymap[i]=SDLK_F11; break;
			case KEY_F12: keymap[i]=SDLK_F12; break;
			case KEY_RO: keymap[i]=SDLK_EURO; break;
			case KEY_KATAKANA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HIRAGANA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HENKAN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KATAKANAHIRAGANA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MUHENKAN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KPJPCOMMA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KPENTER: keymap[i]=SDLK_KP_ENTER; break;
			case KEY_RIGHTCTRL: keymap[i]=SDLK_RCTRL; break;
			case KEY_KPSLASH: keymap[i]=SDLK_KP_DIVIDE; break;
			case KEY_SYSRQ: keymap[i]=SDLK_SYSREQ; break;
			case KEY_RIGHTALT: keymap[i]=SDLK_RALT; break;
			case KEY_LINEFEED: keymap[i]=SDLK_RETURN; break;
			case KEY_HOME: keymap[i]=SDLK_HOME; break;
			case KEY_UP: keymap[i]=SDLK_UP; break;
			case KEY_PAGEUP: keymap[i]=SDLK_PAGEUP; break;
			case KEY_LEFT: keymap[i]=SDLK_LEFT; break;
			case KEY_RIGHT: keymap[i]=SDLK_RIGHT; break;
			case KEY_END: keymap[i]=SDLK_END; break;
			case KEY_DOWN: keymap[i]=SDLK_DOWN; break;
			case KEY_PAGEDOWN: keymap[i]=SDLK_PAGEDOWN; break;
			case KEY_INSERT: keymap[i]=SDLK_INSERT; break;
			case KEY_DELETE: keymap[i]=SDLK_DELETE; break;
			case KEY_MACRO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MUTE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VOLUMEDOWN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VOLUMEUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_POWER: keymap[i]=SDLK_POWER; break;
			case KEY_KPEQUAL: keymap[i]=SDLK_KP_EQUALS; break;
			case KEY_KPPLUSMINUS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PAUSE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KPCOMMA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HANGUEL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HANJA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_YEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_LEFTMETA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RIGHTMETA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_COMPOSE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_STOP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_AGAIN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROPS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_UNDO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FRONT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_COPY: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_OPEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PASTE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FIND: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CUT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HELP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MENU: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CALC: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SETUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SLEEP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_WAKEUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FILE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SENDFILE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DELETEFILE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_XFER: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROG1: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROG2: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_WWW: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MSDOS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SCREENLOCK: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DIRECTION: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CYCLEWINDOWS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MAIL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BOOKMARKS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_COMPUTER: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BACK: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FORWARD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CLOSECD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EJECTCD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EJECTCLOSECD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_NEXTSONG: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PLAYPAUSE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PREVIOUSSONG: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_STOPCD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RECORD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_REWIND: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PHONE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ISO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CONFIG: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HOMEPAGE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_REFRESH: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EXIT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MOVE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EDIT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SCROLLUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SCROLLDOWN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KPLEFTPAREN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KPRIGHTPAREN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F13: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F14: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F15: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F16: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F17: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F18: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F19: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F20: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F21: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F22: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F23: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_F24: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PLAYCD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PAUSECD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROG3: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROG4: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SUSPEND: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CLOSE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PLAY: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FASTFORWARD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BASSBOOST: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PRINT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_HP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CAMERA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SOUND: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_QUESTION: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EMAIL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CHAT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SEARCH: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CONNECT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FINANCE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SPORT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SHOP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ALTERASE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CANCEL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BRIGHTNESSDOWN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BRIGHTNESSUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MEDIA: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_OK: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SELECT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_GOTO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CLEAR: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_POWER2: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_OPTION: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_INFO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TIME: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VENDOR: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ARCHIVE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PROGRAM: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CHANNEL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FAVORITES: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_EPG: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PVR: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MHP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_LANGUAGE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TITLE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SUBTITLE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ANGLE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_ZOOM: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MODE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_KEYBOARD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SCREEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PC: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TV: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TV2: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VCR: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VCR2: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SAT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SAT2: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TAPE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RADIO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TUNER: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PLAYER: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TEXT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DVD: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_AUX: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MP3: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_AUDIO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_VIDEO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DIRECTORY: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_LIST: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MEMO: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CALENDAR: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RED: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_GREEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_YELLOW: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BLUE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CHANNELUP: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_CHANNELDOWN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_FIRST: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_LAST: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_AB: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_NEXT: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_RESTART: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SLOW: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_SHUFFLE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_BREAK: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_PREVIOUS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DIGITS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TEEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_TWEN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DEL_EOL: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DEL_EOS: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_INS_LINE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_DEL_LINE: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_UNKNOWN: keymap[i]=SDLK_UNKNOWN; break;
			case KEY_MAX: keymap[i]=SDLK_UNKNOWN; break;
			default:
				keymap[i]=SDLK_UNKNOWN;
			}
		}
	}
}//exern"C"
