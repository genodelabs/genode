/*
 * \brief  SDL input support
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Linux includes */
#include <SDL/SDL.h>

/* Genode includes */
#include <base/thread.h>
#include <input/keycodes.h>

/* local includes */
#include "input.h"


/**
 * Convert SDL keycode to Genode keycode
 */
static long convert_keycode(int sdl_keycode)
{
	using namespace Input;

	switch (sdl_keycode) {

	case SDLK_BACKSPACE:    return KEY_BACKSPACE;
	case SDLK_TAB:          return KEY_TAB;
	case SDLK_RETURN:       return KEY_ENTER;
	case SDLK_PAUSE:        return KEY_PAUSE;
	case SDLK_ESCAPE:       return KEY_ESC;
	case SDLK_SPACE:        return KEY_SPACE;
	case SDLK_QUOTE:        return KEY_APOSTROPHE;
	case SDLK_HASH:         return KEY_APOSTROPHE;
	case SDLK_COMMA:        return KEY_COMMA;
	case SDLK_MINUS:        return KEY_MINUS;
	case SDLK_PERIOD:       return KEY_DOT;
	case SDLK_SLASH:        return KEY_SLASH;
	case SDLK_0:            return KEY_0;
	case SDLK_1:            return KEY_1;
	case SDLK_2:            return KEY_2;
	case SDLK_3:            return KEY_3;
	case SDLK_4:            return KEY_4;
	case SDLK_5:            return KEY_5;
	case SDLK_6:            return KEY_6;
	case SDLK_7:            return KEY_7;
	case SDLK_8:            return KEY_8;
	case SDLK_9:            return KEY_9;
	case SDLK_SEMICOLON:    return KEY_SEMICOLON;
	case SDLK_LESS:         return KEY_BACKSLASH;
	case SDLK_EQUALS:       return KEY_EQUAL;
	case SDLK_QUESTION:     return KEY_QUESTION;
	case SDLK_LEFTBRACKET:  return KEY_LEFTBRACE;
	case SDLK_BACKSLASH:    return KEY_BACKSLASH;
	case SDLK_RIGHTBRACKET: return KEY_RIGHTBRACE;
	case SDLK_BACKQUOTE:    return KEY_GRAVE;
	case SDLK_a:            return KEY_A;
	case SDLK_b:            return KEY_B;
	case SDLK_c:            return KEY_C;
	case SDLK_d:            return KEY_D;
	case SDLK_e:            return KEY_E;
	case SDLK_f:            return KEY_F;
	case SDLK_g:            return KEY_G;
	case SDLK_h:            return KEY_H;
	case SDLK_i:            return KEY_I;
	case SDLK_j:            return KEY_J;
	case SDLK_k:            return KEY_K;
	case SDLK_l:            return KEY_L;
	case SDLK_m:            return KEY_M;
	case SDLK_n:            return KEY_N;
	case SDLK_o:            return KEY_O;
	case SDLK_p:            return KEY_P;
	case SDLK_q:            return KEY_Q;
	case SDLK_r:            return KEY_R;
	case SDLK_s:            return KEY_S;
	case SDLK_t:            return KEY_T;
	case SDLK_u:            return KEY_U;
	case SDLK_v:            return KEY_V;
	case SDLK_w:            return KEY_W;
	case SDLK_x:            return KEY_X;
	case SDLK_y:            return KEY_Y;
	case SDLK_z:            return KEY_Z;
	case SDLK_DELETE:       return KEY_DELETE;

	/* arrows + home/end pad */
	case SDLK_UP:           return KEY_UP;
	case SDLK_DOWN:         return KEY_DOWN;
	case SDLK_RIGHT:        return KEY_RIGHT;
	case SDLK_LEFT:         return KEY_LEFT;
	case SDLK_INSERT:       return KEY_INSERT;
	case SDLK_HOME:         return KEY_HOME;
	case SDLK_END:          return KEY_END;
	case SDLK_PAGEUP:       return KEY_PAGEUP;
	case SDLK_PAGEDOWN:     return KEY_PAGEDOWN;

	/* function keys */
	case SDLK_F1:           return KEY_F1;
	case SDLK_F2:           return KEY_F2;
	case SDLK_F3:           return KEY_F3;
	case SDLK_F4:           return KEY_F4;
	case SDLK_F5:           return KEY_F5;
	case SDLK_F6:           return KEY_F6;
	case SDLK_F7:           return KEY_F7;
	case SDLK_F8:           return KEY_F8;
	case SDLK_F9:           return KEY_F9;
	case SDLK_F10:          return KEY_F10;
	case SDLK_F11:          return KEY_F11;
	case SDLK_F12:          return KEY_F12;
	case SDLK_F13:          return KEY_F13;
	case SDLK_F14:          return KEY_F14;
	case SDLK_F15:          return KEY_F15;

	/* special keys */
	case SDLK_PRINT:        return KEY_PRINT;
	case SDLK_SCROLLOCK:    return KEY_SCROLLLOCK;
	case SDLK_MENU:         return KEY_MENU;

	/* key state modifier keys */
	case SDLK_NUMLOCK:      return KEY_NUMLOCK;
	case SDLK_CAPSLOCK:     return KEY_CAPSLOCK;
	case SDLK_RSHIFT:       return KEY_RIGHTSHIFT;
	case SDLK_LSHIFT:       return KEY_LEFTSHIFT;
	case SDLK_RCTRL:        return KEY_RIGHTCTRL;
	case SDLK_LCTRL:        return KEY_LEFTCTRL;
	case SDLK_RALT:         return KEY_RIGHTALT;
	case SDLK_LALT:         return KEY_LEFTALT;
	case SDLK_RMETA:        return KEY_RIGHTMETA;
	case SDLK_LMETA:        return KEY_LEFTMETA;

	default:                return 0;
	}
};


static Input::Event wait_for_sdl_event()
{
	using namespace Input;

	SDL_Event event;
	static int mx, my;
	static int ox, oy;

	SDL_WaitEvent(&event);

	/* query new mouse position */
	ox = mx; oy = my;
	if (event.type == SDL_MOUSEMOTION)
		SDL_GetMouseState(&mx, &my);

	/* determine keycode */
	long keycode = 0;
	switch (event.type) {
	case SDL_KEYUP:
	case SDL_KEYDOWN:

		keycode = convert_keycode(event.key.keysym.sym);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:

		switch (event.button.button) {
		case SDL_BUTTON_LEFT:   keycode = BTN_LEFT;   break;
		case SDL_BUTTON_MIDDLE: keycode = BTN_MIDDLE; break;
		case SDL_BUTTON_RIGHT:  keycode = BTN_RIGHT;  break;
		default:                keycode = 0;
		}
	}

	/* determine event type */
	Event::Type type;
	switch (event.type) {
	case SDL_MOUSEMOTION:
		type = Event::MOTION;
		break;

	case SDL_KEYUP:
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_WHEELUP
		 || event.button.button == SDL_BUTTON_WHEELDOWN)
			/* ignore */
			return Event();

		type = Event::RELEASE;
		break;

	case SDL_KEYDOWN:
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_WHEELUP) {
			type = Event::WHEEL;
			return Event(type, 0, 0, 0, 0, 1);
		} else if (event.button.button == SDL_BUTTON_WHEELDOWN) {
			type = Event::WHEEL;
			return Event(type, 0, 0, 0, 0, -1);
		}
		type = Event::PRESS;
		break;

	default:
		return Event();
	}

	return Event(type, keycode, mx, my, mx - ox, my - oy);
}


namespace Input {
	struct Backend;
}

struct Input::Backend : Genode::Thread
{
	Handler &handler;

	Backend(Genode::Env &env, Input::Handler &handler)
	:
		Genode::Thread(env, "input_backend", 4 * 1024 * sizeof(long)),
		handler(handler)
	{
		start();
	}

	void entry()
	{
		while (true) {
			Input::Event e;

			/* prevent flooding of client with invalid events */
			do {
				e = wait_for_sdl_event();
			} while (e.type() == Input::Event::INVALID);

			handler.event(e);
		}
	}
};


void init_input_backend(Genode::Env &env, Input::Handler &h)
{
	static Input::Backend inst(env, h);
}
