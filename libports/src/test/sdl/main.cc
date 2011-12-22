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

/* the attributes of the screen */
const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 768;
const int SCREEN_BPP = 16;

int main( int argc, char* args[] )
{
	/* start SDL */
	if(SDL_Init(SDL_INIT_VIDEO) == -1)
	{
		PWRN("SDL_Init returned evil! ...");
		return 1;
	}

	/* set up the screen */
	SDL_Surface *screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_SWSURFACE);
	if(screen == 0)
		return 1;

	/* paint something into pixel buffer */
	short* pixels = (short*) screen->pixels;
	for (int i = 0; i < SCREEN_HEIGHT; i++)
		for (int j = 0; j < SCREEN_WIDTH; j++)
			pixels[i*SCREEN_WIDTH+j] = (i/32)*32*64 + (j/32)*32 + i*j/1024;
	SDL_UpdateRect(screen, 0, 0, 0, 0);

	bool done = false;
	while (!done) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_KEYDOWN:
				printf( "%s\n", SDL_GetKeyName(event.key.keysym.sym));
				done = true;
				break;
			}
		}
	}

	/* quit SDL */
	SDL_Quit();
	return 0;
}
