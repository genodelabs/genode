/*
 * \brief  SDL3-to-2 compability layer for the SDL implementation of the Genode framebuffer
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

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__SDL3__SDL_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__SDL3__SDL_H_

/* Linux includes */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wnarrowing"        /* arm_neon.h */
#pragma GCC diagnostic ignored "-Wunused-parameter" /* arm_neon.h */
#pragma GCC diagnostic ignored "-Wfloat-conversion" /* arm_neon.h */
#include <SDL3/SDL.h>
#include <math.h>
#undef SDL_TRUE
#define SDL_TRUE true
#undef SDL_FALSE
#define SDL_FALSE false
#pragma GCC diagnostic pop


namespace Fb_sdl {

using MousePosition = float;

static inline bool is_window_event(const SDL_Event& event)
{
	return event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST;
}

static inline bool is_window_resized_event(const SDL_Event& event)
{
	return event.type == SDL_EVENT_WINDOW_RESIZED;
}

static inline bool InitSDL()
{
	if(!SDL_Init(SDL_INIT_VIDEO)) {
		return false;
	}
	SDL_HideCursor();
	return true;
}

static inline SDL_Window* CreateWindow(const char* title, int width, int height, int window_flags)
{
	SDL_Window* const window_ptr = SDL_CreateWindow(title, width, height, window_flags);
	SDL_SetWindowResizable(window_ptr, true);
	return window_ptr;
}

static inline Uint32 GetTicks()
{
	return static_cast<Uint32>(SDL_GetTicks());
}

static inline SDL_Surface* CreateSurface(int width, int height)
{
	unsigned const bpp        = 32;
	unsigned const red_mask   = 0x00FF0000;
	unsigned const green_mask = 0x0000FF00;
	unsigned const blue_mask  = 0x000000FF;
	unsigned const alpha_mask = 0xFF000000;
	auto format = SDL_GetPixelFormatForMasks(bpp, red_mask, green_mask, blue_mask, alpha_mask);
	return SDL_CreateSurface(width, height, format);
}

static inline SDL_Renderer* CreateRenderer(SDL_Window* const window_ptr)
{
	return SDL_CreateRenderer(window_ptr, "software");
}

static inline SDL_Texture* CreateTexture(SDL_Renderer* const renderer_ptr, int width, int height)
{
	return SDL_CreateTexture(renderer_ptr, SDL_PIXELFORMAT_XRGB8888,
	                         SDL_TEXTUREACCESS_STREAMING,
	                         width, height);
}

static inline void RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture,
                              const SDL_Rect* src, const SDL_Rect* dst)
{
	SDL_FRect fsrc, fdst;
	SDL_RectToFRect(src, &fsrc);
	SDL_RectToFRect(dst, &fdst);
	SDL_RenderTexture(renderer, texture, &fsrc, &fdst);
}

static inline void FreeSurface(SDL_Surface* surface)
{
	SDL_DestroySurface(surface);
}

}

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__SDL__SDL_H_ */
