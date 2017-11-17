/*
 * \brief  Simple SDL test program
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

/* SDL includes */
#include <SDL/SDL.h>

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <timer_session/connection.h>


static void draw(SDL_Surface * const screen, int w, int h)
{
	if (screen == nullptr) { return; }

	/* paint something into pixel buffer */
	short* const pixels = (short* const) screen->pixels;
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			pixels[i*w+j] = (i/32)*32*64 + (j/32)*32 + i*j/1024;
		}
	}
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}


static SDL_Surface *set_video_mode(int w, int h)
{
	SDL_Surface *screen = SDL_SetVideoMode(w, h, 16, SDL_SWSURFACE);
	if (screen == nullptr) {
		printf("Error: could not set video mode: %s\n", SDL_GetError());
	}

	return screen;
}


static SDL_Surface *resize_screen(SDL_Surface * const screen, int w, int h)
{
	if (screen == nullptr) { return nullptr; }

	int oldw = screen->w;
	int oldh = screen->h;

	SDL_Surface *nscreen = set_video_mode(w, h);
	if (nscreen == nullptr) {
		printf("Error: could not resize %dx%d -> %dx%d: %s\n",
		       oldw, oldh, w, h, SDL_GetError());
		return nullptr;
	}

	return nscreen;
}


int main( int argc, char* args[] )
{

	/* start SDL */
	if (SDL_Init(SDL_INIT_VIDEO) == -1) {
		printf("Error: could not initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	/* set screen to max WxH */
	SDL_Surface *screen = set_video_mode(0, 0);
	if (screen == nullptr) { return 1; }

	bool done = false;
	while (!done) {
		draw(screen, screen->w, screen->h);
		SDL_Delay(10);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_KEYDOWN:
				printf( "%s\n", SDL_GetKeyName(event.key.keysym.sym));
				done = true;
				break;
			case SDL_VIDEORESIZE:
				screen = resize_screen(screen, event.resize.w, event.resize.h);
				if (screen == nullptr) { done = true; }

				break;
			}
		}
	}

	/* quit SDL */
	SDL_Quit();
	return 0;
}
