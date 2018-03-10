/*
 * \brief  Genode-specific video backend
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
#include <base/env.h>
#include <framebuffer_session/connection.h>

/* local includes */
#include <SDL_genode_internal.h>


extern Genode::Env        *global_env();

extern Genode::Lock event_lock;
extern Video        video_events;


extern "C" {

#include <dlfcn.h>

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>
#include <SDL/SDL_mouse.h>
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"
#include "SDL_genode_fb_events.h"
#include "SDL_genode_fb_video.h"

	static SDL_Rect df_mode;

	struct Sdl_framebuffer
	{
		Genode::Env        &_env;

		Framebuffer::Mode _mode;
		Framebuffer::Connection _fb { _env, _mode };

		void _handle_mode_change()
		{
			Genode::Lock_guard<Genode::Lock> guard(event_lock);

			Framebuffer::Mode mode = _fb.mode();
			df_mode.w = mode.width();
			df_mode.h = mode.height();

			video_events.resize_pending = true;
			video_events.width  = mode.width();
			video_events.height = mode.height();
		}

		Genode::Signal_handler<Sdl_framebuffer> _mode_handler {
			_env.ep(), *this, &Sdl_framebuffer::_handle_mode_change };

		Sdl_framebuffer(Genode::Env &env) : _env(env) {
			_fb.mode_sigh(_mode_handler); }

		bool valid() const { return _fb.cap().valid(); }


		/************************************
		 ** Framebuffer::Session Interface **
		 ************************************/

		Genode::Dataspace_capability dataspace() { return _fb.dataspace(); }

		Framebuffer::Mode mode() const { return _fb.mode(); }

		void refresh(int x, int y, int w, int h) {
			_fb.refresh(x, y, w, h); }
	};

	static Genode::Constructible<Sdl_framebuffer> framebuffer;
	static Framebuffer::Mode scr_mode;
	static SDL_Rect *modes[2];

#if defined(SDL_VIDEO_OPENGL)

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define MAX_CONFIGS 10
#define MAX_MODES 100

	/**********************************
	 ** EGL/OpenGL backend functions **
	 **********************************/

	static EGLDisplay display;
	static EGLSurface screen_surf;
	static EGLNativeWindowType native_window;

	typedef EGLBoolean  (*eglBindAPI_func)             (EGLenum);
	typedef EGLBoolean  (*eglChooseConfig_func)        (EGLDisplay, const EGLint *, EGLConfig *, EGLint , EGLint *);
	typedef EGLContext  (*eglCreateContext_func)       (EGLDisplay, EGLConfig, EGLContext, const EGLint *);
	typedef EGLSurface  (*eglCreatePixmapSurface_func) (EGLDisplay, EGLConfig, EGLNativePixmapType, const EGLint *);
	typedef EGLDisplay  (*eglGetDisplay_func)          (EGLNativeDisplayType);
	typedef EGLBoolean  (*eglInitialize_func)          (EGLDisplay, EGLint *, EGLint *);
	typedef EGLBoolean  (*eglMakeCurrent_func)         (EGLDisplay, EGLSurface, EGLSurface, EGLContext);
	typedef EGLBoolean  (*eglSwapBuffers_func)         (EGLDisplay, EGLSurface);
	typedef EGLBoolean  (*eglWaitClient_func)          (void);
	typedef char const* (*eglQueryString_func)         (EGLDisplay, EGLint);
	typedef __eglMustCastToProperFunctionPointerType
	                    (*eglGetProcAddress_func)      (const char *procname);


	static eglBindAPI_func             __eglBindAPI;
	static eglChooseConfig_func        __eglChooseConfig;
	static eglCreateContext_func       __eglCreateContext;
	static eglCreatePixmapSurface_func __eglCreatePixmapSurface;
	static eglGetDisplay_func          __eglGetDisplay;
	static eglInitialize_func          __eglInitialize;
	static eglMakeCurrent_func         __eglMakeCurrent;
	static eglSwapBuffers_func         __eglSwapBuffers;
	static eglWaitClient_func          __eglWaitClient;
	static eglQueryString_func         __eglQueryString;
	static eglGetProcAddress_func      __eglGetProcAddress;

	static bool init_egl()
	{
		void *egl = dlopen("egl.lib.so", 0);
		if (!egl) {
			Genode::error("could not open EGL library");
			return false;
		}

#define LOAD_GL_FUNC(lib, sym) \
	__ ## sym = (sym ##_func) dlsym(lib, #sym); \
	if (!__ ## sym) { return false; }

		LOAD_GL_FUNC(egl, eglBindAPI)
		LOAD_GL_FUNC(egl, eglChooseConfig)
		LOAD_GL_FUNC(egl, eglCreateContext)
		LOAD_GL_FUNC(egl, eglCreatePixmapSurface)
		LOAD_GL_FUNC(egl, eglGetDisplay)
		LOAD_GL_FUNC(egl, eglInitialize)
		LOAD_GL_FUNC(egl, eglMakeCurrent)
		LOAD_GL_FUNC(egl, eglQueryString)
		LOAD_GL_FUNC(egl, eglSwapBuffers)
		LOAD_GL_FUNC(egl, eglWaitClient)
		LOAD_GL_FUNC(egl, eglGetProcAddress)

#undef LOAD_GL_FUNC

		return true;
	}

	static bool init_opengl(SDL_VideoDevice *t)
	{
		if (!init_egl()) { return false; }

		int maj, min;
		EGLContext ctx;
		EGLConfig configs[MAX_CONFIGS];
		GLboolean printInfo = GL_FALSE;

		display = __eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (!display) {
			Genode::error("eglGetDisplay failed\n");
			return false;
		}

		if (!__eglInitialize(display, &maj, &min)) {
			Genode::error("eglInitialize failed\n");
			return false;
		}

		Genode::log("EGL version = ", maj, ".", min);
		Genode::log("EGL_VENDOR = ", __eglQueryString(display, EGL_VENDOR));

		EGLConfig config;
		EGLint config_attribs[32];
		EGLint renderable_type, num_configs, i;

		i = 0;
		config_attribs[i++] = EGL_RED_SIZE;
		config_attribs[i++] = 1;
		config_attribs[i++] = EGL_GREEN_SIZE;
		config_attribs[i++] = 1;
		config_attribs[i++] = EGL_BLUE_SIZE;
		config_attribs[i++] = 1;
		config_attribs[i++] = EGL_DEPTH_SIZE;
		config_attribs[i++] = 1;

		config_attribs[i++] = EGL_SURFACE_TYPE;
		config_attribs[i++] = EGL_WINDOW_BIT;;

		config_attribs[i++] = EGL_RENDERABLE_TYPE;
		renderable_type = 0x0;
		renderable_type |= EGL_OPENGL_BIT;
		config_attribs[i++] = renderable_type;

		config_attribs[i] = EGL_NONE;

		if (!__eglChooseConfig(display, config_attribs, &config, 1, &num_configs)
		    || !num_configs) {
			Genode::error("eglChooseConfig failed");
			return false;
		}

		__eglBindAPI(EGL_OPENGL_API);

		EGLint context_attribs[4]; context_attribs[0] = EGL_NONE;
		ctx = __eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
		if (!ctx) {
			Genode::error("eglCreateContext failed");
			return false;
		}

		Genode_egl_window egl_window { scr_mode.width(), scr_mode.height(),
		                               (unsigned char*)t->hidden->buffer };

		screen_surf = __eglCreatePixmapSurface(display, config, &egl_window, NULL);
		if (screen_surf == EGL_NO_SURFACE) {
			Genode::error("eglCreatePixmapSurface failed");
			return false;
		}

		if (!__eglMakeCurrent(display, screen_surf, screen_surf, ctx)) {
			Genode::error("eglMakeCurrent failed");
			return false;
		}

		t->gl_config.driver_loaded = 1;
		return true;
	}
#endif

	/****************************************
	 * Genode_Fb driver bootstrap functions *
	 ****************************************/

	static int Genode_Fb_Available(void)
	{
		if (!framebuffer.constructed()) {
			framebuffer.construct(*global_env());
		}

		if (!framebuffer->valid()) {
			Genode::error("could not obtain framebuffer session");
			return 0;
		}

		return 1;
	}


	static void Genode_Fb_DeleteDevice(SDL_VideoDevice *device)
	{
		if (framebuffer.constructed()) {
			framebuffer.destruct();
		}
	}


	static SDL_VideoDevice *Genode_Fb_CreateDevice(int devindex)
	{
		SDL_VideoDevice *device;

		/* Initialize all variables that we clean on shutdown */
		device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
		if ( device ) {
			SDL_memset(device, 0, (sizeof *device));
			device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
		}
		if ( (device == 0) || (device->hidden == 0) ) {
			SDL_OutOfMemory();
			if ( device ) {
				SDL_free(device);
			}
			return(0);
		}
		SDL_memset(device->hidden, 0, (sizeof *device->hidden));

		/* Set the function pointers */
		device->VideoInit        = Genode_Fb_VideoInit;
		device->ListModes        = Genode_Fb_ListModes;
		device->SetVideoMode     = Genode_Fb_SetVideoMode;
		device->SetColors        = Genode_Fb_SetColors;
		device->UpdateRects      = Genode_Fb_UpdateRects;
		device->VideoQuit        = Genode_Fb_VideoQuit;
		device->AllocHWSurface   = Genode_Fb_AllocHWSurface;
		device->LockHWSurface    = Genode_Fb_LockHWSurface;
		device->UnlockHWSurface  = Genode_Fb_UnlockHWSurface;
		device->FreeHWSurface    = Genode_Fb_FreeHWSurface;
		device->InitOSKeymap     = Genode_Fb_InitOSKeymap;
		device->PumpEvents       = Genode_Fb_PumpEvents;
		device->free             = Genode_Fb_DeleteDevice;
		device->CreateYUVOverlay = 0;
		device->CheckHWBlit      = 0;
		device->FillHWRect       = 0;
		device->SetHWColorKey    = 0;
		device->SetHWAlpha       = 0;
		device->FlipHWSurface    = 0;
		device->SetCaption       = 0;
		device->SetIcon          = 0;
		device->IconifyWindow    = 0;
		device->GrabInput        = 0;
		device->GetWMInfo        = 0;

		device->GL_MakeCurrent    = Genode_Fb_GL_MakeCurrent;
		device->GL_SwapBuffers    = Genode_Fb_GL_SwapBuffers;
		device->GL_LoadLibrary    = Genode_Fb_GL_LoadLibrary;
		device->GL_GetProcAddress = Genode_Fb_GL_GetProcAddress;
		return device;
	}


	VideoBootStrap Genode_fb_bootstrap = {
		"Genode_Fb", "SDL genode_fb video driver",
		Genode_Fb_Available, Genode_Fb_CreateDevice
	};


	/*****************
	 * Functionality
	 ****************/

	/**
	 * Initialize the native video subsystem, filling 'vformat' with the
	 * "best" display pixel format, returning 0 or -1 if there's an error.
	 */
	int Genode_Fb_VideoInit(SDL_VideoDevice *t, SDL_PixelFormat *vformat)
	{
		if (!framebuffer.constructed()) {
			Genode::error("framebuffer not initialized");
			return -1;
		}

		/* Get the framebuffer size and mode infos */
		scr_mode = framebuffer->mode();
		t->info.current_w = scr_mode.width();
		t->info.current_h = scr_mode.height();
		Genode::log("Framebuffer has "
		            "width=",  t->info.current_w, " "
		            "height=", t->info.current_h);

		/* set mode specific values */
		switch(scr_mode.format())
		{
		case Framebuffer::Mode::RGB565:
			Genode::log("We use pixelformat rgb565.");
			vformat->BitsPerPixel  = 16;
			vformat->BytesPerPixel = scr_mode.bytes_per_pixel();
			vformat->Rmask = 0x0000f800;
			vformat->Gmask = 0x000007e0;
			vformat->Bmask = 0x0000001f;
			break;
		default:
			SDL_SetError("Couldn't get console mode info");
			Genode_Fb_VideoQuit(t);
			return -1;
		}
		modes[0] = &df_mode;
		df_mode.w = scr_mode.width();
		df_mode.h = scr_mode.height();
		modes[1] = 0;

		t->hidden->buffer = 0;
		return 0;
	}


	/**
	 *Note:  If we are terminated, this could be called in the middle of
	 * another SDL video routine -- notably UpdateRects.
	 */
	void Genode_Fb_VideoQuit(SDL_VideoDevice *t)
	{
		Genode::log("Quit video device ...");

		if (t->screen->pixels) {
			t->screen->pixels = nullptr;
		}

		if (t->hidden->buffer) {
			global_env()->rm().detach(t->hidden->buffer);
			t->hidden->buffer = nullptr;
		}
	}


	/**
	 * List the available video modes for the given pixel format,
	 * sorted from largest to smallest.
	 */
	SDL_Rect **Genode_Fb_ListModes(SDL_VideoDevice *t,
	                               SDL_PixelFormat *format,
	                               Uint32 flags)
	{
		if(format->BitsPerPixel != 16) { return (SDL_Rect **) 0; }
		return modes;
	}


	/**
	 * Set the requested video mode, returning a surface which will be
	 * set to the SDL_VideoSurface.  The width and height will already
	 * be verified by ListModes(), and the video subsystem is free to
	 * set the mode to a supported bit depth different from the one
	 * specified -- the desired bpp will be emulated with a shadow
	 * surface if necessary.  If a new mode is returned, this function
	 * should take care of cleaning up the current mode.
	 */
	SDL_Surface *Genode_Fb_SetVideoMode(SDL_VideoDevice *t,
	                                    SDL_Surface *current,
	                                    int width, int height,
	                                    int bpp, Uint32 flags)
	{
		/* for now we do not support this */
		if (t->hidden->buffer && flags & SDL_OPENGL) {
			Genode::error("resizing a OpenGL window not possible");
			return nullptr;
		}

		/*
		 * XXX there is something wrong with how this is used now.
		 *     SDL_Flip() is going to call FlipHWSurface which was never
		 *     implemented and leads to a nullptr function call.
		 */
		if (flags & SDL_DOUBLEBUF) {
			Genode::warning("disable requested double-buffering");
			flags &= ~SDL_DOUBLEBUF;
		}

		/* Map the buffer */
		Genode::Dataspace_capability fb_ds_cap = framebuffer->dataspace();
		if (!fb_ds_cap.valid()) {
			Genode::error("could not request dataspace for frame buffer");
			return nullptr;
		}

		if (t->hidden->buffer) {
			global_env()->rm().detach(t->hidden->buffer);
		}

		t->hidden->buffer = global_env()->rm().attach(fb_ds_cap);

		if (!t->hidden->buffer) {
			Genode::error("no buffer for requested mode");
			return nullptr;
		}

		Genode::log("Set video mode to: ", width, "x", height, "@", bpp);

		SDL_memset(t->hidden->buffer, 0, width * height * (bpp / 8));

		if (!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0) ) {
			Genode::error("couldn't allocate new pixel format for requested mode");
			return nullptr;
		}

		/* Set up the new mode framebuffer */
		current->flags = flags | SDL_FULLSCREEN;
		t->hidden->w = current->w = width;
		t->hidden->h = current->h = height;
		current->pitch = current->w * (bpp / 8);

#if defined(SDL_VIDEO_OPENGL)
		if ((flags & SDL_OPENGL) && !init_opengl(t)) {
			return nullptr;
		}
#endif

		/*
		 * XXX if SDL ever wants to free the pixels pointer,
		 *     free() in the libc will trigger a page-fault
		 */
		current->pixels = t->hidden->buffer;
		return current;
	}


	/**
	 * We don't actually allow hardware surfaces other than the main one
	 */
	static int Genode_Fb_AllocHWSurface(SDL_VideoDevice *t,
	                                    SDL_Surface *surface)
	{
		Genode::log(__func__, " not supported yet ...");
		return -1;
	}


	static void Genode_Fb_FreeHWSurface(SDL_VideoDevice *t,
	                                    SDL_Surface *surface)
	{
		Genode::log(__func__, " not supported yet ...");
	}


	/**
	 * We need to wait for vertical retrace on page flipped displays
	 */
	static int Genode_Fb_LockHWSurface(SDL_VideoDevice *t,
	                                   SDL_Surface *surface)
	{
		/* Genode::log(__func__, " not supported yet ..."); */
		return 0;
	}


	static void Genode_Fb_UnlockHWSurface(SDL_VideoDevice *t,
	                                      SDL_Surface *surface)
	{
		/* Genode::log(__func__, " not supported yet ..."); */
	}


	static void Genode_Fb_UpdateRects(SDL_VideoDevice *t, int numrects,
	                                  SDL_Rect *rects)
	{
		int i;
		for(i=0;i<numrects;i++) {
			framebuffer->refresh(rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		}
	}


	/**
	 * Sets the color entries { firstcolor .. (firstcolor+ncolors-1) }
	 * of the physical palette to those in 'colors'. If the device is
	 * using a software palette (SDL_HWPALETTE not set), then the
	 * changes are reflected in the logical palette of the screen
	 * as well.
	 * The return value is 1 if all entries could be set properly
	 * or 0 otherwise.
	 */
	int Genode_Fb_SetColors(SDL_VideoDevice *t, int firstcolor,
	                        int ncolors, SDL_Color *colors)
	{
		Genode::warning(__func__, " not yet implemented");
		return 1;
	}


	int Genode_Fb_GL_MakeCurrent(SDL_VideoDevice *t)
	{
		Genode::warning(__func__, ": not yet implemented");
		return 0;
	}


	void Genode_Fb_GL_SwapBuffers(SDL_VideoDevice *t)
	{
#if defined(SDL_VIDEO_OPENGL)
		__eglWaitClient();
		__eglSwapBuffers(display, screen_surf);
		framebuffer->refresh(0, 0, scr_mode.width(), scr_mode.height());
#endif
	}

	int Genode_Fb_GL_LoadLibrary(SDL_VideoDevice *t, const char *path)
	{
		Genode::warning(__func__, ": not yet implemented");
		return 0;
	}


	void* Genode_Fb_GL_GetProcAddress(SDL_VideoDevice *t, const char *proc) {

		return (void*)__eglGetProcAddress(proc); }
} //extern "C"
