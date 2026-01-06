/*
 * \brief  SDL2 layer for the SDL implementation of the Genode framebuffer
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2026-01-07
 */

/*
 * Copyright (C) 2006-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__SDL2__SDL_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__SDL2__SDL_H_

/* Linux includes */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wnarrowing"        /* arm_neon.h */
#pragma GCC diagnostic ignored "-Wunused-parameter" /* arm_neon.h */
#pragma GCC diagnostic ignored "-Wfloat-conversion" /* arm_neon.h */
#include <SDL2/SDL.h>
#pragma GCC diagnostic pop


namespace Fb_sdl {

using MousePosition = int;

#define SDL_EVENT_USER SDL_USEREVENT
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#define SDL_EVENT_KEY_UP SDL_KEYUP
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL

static inline bool is_window_event(const SDL_Event& event)
{
	return event.type == SDL_WINDOWEVENT;
}

static inline bool is_window_resized_event(const SDL_Event& event)
{
	return event.window.event == SDL_WINDOWEVENT_RESIZED;
}

static inline bool InitSDL()
{
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		return false;
	}
	SDL_ShowCursor(0);
	return true;
}

static inline SDL_Window* CreateWindow(const char* title, int width, int height, int window_flags)
{
	SDL_Window* const window_ptr = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                                                width, height, window_flags);
	SDL_SetWindowResizable(window_ptr, SDL_TRUE);
	return window_ptr;
}

static inline Uint32 GetTicks()
{
	return SDL_GetTicks();
}

static inline SDL_Surface* CreateSurface(int width, int height)
{
	unsigned const flags      = 0;
	unsigned const bpp        = 32;
	unsigned const red_mask   = 0x00FF0000;
	unsigned const green_mask = 0x0000FF00;
	unsigned const blue_mask  = 0x000000FF;
	unsigned const alpha_mask = 0xFF000000;
	return SDL_CreateRGBSurface(flags, width, height , bpp,
	                            red_mask, green_mask, blue_mask, alpha_mask);
}

static inline SDL_Renderer* CreateRenderer(SDL_Window* const window_ptr)
{
	int const index = -1;
	unsigned const renderer_flags = SDL_RENDERER_SOFTWARE;
	SDL_Renderer *renderer_ptr = SDL_CreateRenderer(window_ptr, index, renderer_flags);
	return renderer_ptr;
}

static inline SDL_Texture* CreateTexture(SDL_Renderer* const renderer_ptr, int width, int height)
{
	return SDL_CreateTexture(renderer_ptr, SDL_PIXELFORMAT_ARGB8888,
	                         SDL_TEXTUREACCESS_STREAMING,
	                         width, height);
}

static inline void RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture,
                              const SDL_Rect* src, const SDL_Rect* dst)
{
	SDL_RenderCopy(renderer, texture, src, dst);
}

static inline void FreeSurface(SDL_Surface* surface) {
	SDL_FreeSurface(surface);
}

}

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__SDL2__SDL_H_ */
