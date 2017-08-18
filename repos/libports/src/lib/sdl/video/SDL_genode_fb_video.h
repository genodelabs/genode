/*
 * \brief  Genode-specific video backend header
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
#ifndef _SDL_genode_fb_video_h
#define _SDL_genode_fb_video_h

/* Private display data */
struct SDL_PrivateVideoData {
    int w, h;
    void *buffer;
};

/**
 * Initialization/Query functions
 */
static int Genode_Fb_VideoInit(SDL_VideoDevice *t, SDL_PixelFormat *vformat);
static SDL_Rect **Genode_Fb_ListModes(SDL_VideoDevice *t,
                                      SDL_PixelFormat *format,
                                      Uint32 flags);
static SDL_Surface *Genode_Fb_SetVideoMode(SDL_VideoDevice *t,
                                           SDL_Surface *current,
                                           int width, int height,
                                           int bpp, Uint32 flags);
static int Genode_Fb_SetColors(SDL_VideoDevice *t, int firstcolor,
                               int ncolors, SDL_Color *colors);
static void Genode_Fb_VideoQuit(SDL_VideoDevice *t);

/**
 * Hardware surface functions
 */
static int Genode_Fb_AllocHWSurface(SDL_VideoDevice *t, SDL_Surface *surface);
static int Genode_Fb_LockHWSurface(SDL_VideoDevice *t, SDL_Surface *surface);
static void Genode_Fb_UnlockHWSurface(SDL_VideoDevice *t, SDL_Surface *surface);
static void Genode_Fb_FreeHWSurface(SDL_VideoDevice *t, SDL_Surface *surface);

/**
 * etc.
 */
static void Genode_Fb_UpdateRects(SDL_VideoDevice *t, int numrects,
                                  SDL_Rect *rects);

/**
 * OpenGL functions
 */
static int   Genode_Fb_GL_MakeCurrent(SDL_VideoDevice *t);
static void  Genode_Fb_GL_SwapBuffers(SDL_VideoDevice *t);
static int   Genode_Fb_GL_LoadLibrary(SDL_VideoDevice *t, const char *path);
static void* Genode_Fb_GL_GetProcAddress(SDL_VideoDevice *t, const char *proc);

#endif // _SDL_genode_fb_video_h
