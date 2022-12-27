/*
 * \brief  GLX/X11 emulation for SVGA3D
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2021-10-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>

/* GLX/X11 includes */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

/* VirtualBox includes */
#include <iprt/types.h>
#include <iprt/err.h>

/* local includes */
#include <stub_macros.h>

static bool const debug = false;

using namespace Genode;


/* from VBoxSVGA3DLazyLoad.asm */
extern "C" int ExplicitlyLoadVBoxSVGA3D(bool fResolveAllImports, PRTERRINFO pErrInfo) TRACE(VINF_SUCCESS);


/*
 * GLX
 */

extern "C" GLXFBConfig * glXChooseFBConfig(Display *dpy, int screen, const int *attribList, int *nitems) STOP
extern "C" int           glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value) STOP
extern "C" XVisualInfo * glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config) STOP


typedef void (*__GLXextFuncPtr)(void);

__GLXextFuncPtr glXGetProcAddress(GLubyte const *procname)
{
	return (__GLXextFuncPtr)eglGetProcAddress((char const *)procname);
}


extern "C"
Bool glXQueryVersion(Display *display, int *major, int *minor)
{
	EGLBoolean initialized = eglInitialize(display->dpy, major, minor);

	if (initialized)
		Genode::log("EGL_VERSION = ", eglQueryString(display->dpy, EGL_VERSION));

	return initialized;
}


extern "C"
XVisualInfo * glXChooseVisual(Display *display, int screen, int *attribList)
{
	EGLConfig config;
	EGLint config_attribs[32];
	EGLint num_configs, i;

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
	config_attribs[i++] = EGL_WINDOW_BIT;
	config_attribs[i++] = EGL_RENDERABLE_TYPE;
	config_attribs[i++] = EGL_OPENGL_BIT;
	config_attribs[i] = EGL_NONE;

	if (!eglChooseConfig(display->dpy, config_attribs, &config, 1, &num_configs)
	                     || !num_configs) {
		Genode::error("failed to choose a config");
		return nullptr;
	}

	static XVisualInfo info { };
	static Visual visual { };
	visual.config = config;
	info.visual = &visual;

	return &info;
}


extern "C"
GLXContext glXCreateContext(Display *display, XVisualInfo *vis,
                            GLXContext shareList, Bool direct)
{
	eglBindAPI(EGL_OPENGL_API);
	EGLint context_attribs[1] { EGL_NONE };

	GLXContext ctx = new _GLXContext();
	ctx->context = eglCreateContext(display->dpy, vis->visual->config,
	                                shareList ? shareList->context : EGL_NO_CONTEXT,
	                                context_attribs);
	if (!ctx->context) {
		Genode::error("failed to create context");
		return nullptr;
	}

	return ctx;
}


extern "C"
Bool glXMakeCurrent(Display *display, GLXDrawable drawable, GLXContext ctx)
{
	if (!eglMakeCurrent(display->dpy,
	                    drawable ? drawable->surface : EGL_NO_SURFACE,
	                    drawable ? drawable->surface : EGL_NO_SURFACE,
	                    ctx ? ctx->context : EGL_NO_CONTEXT)) {
		Genode::error("failed to make current drawable");
		return False;
	}

	//Genode::warning("glXMakeCurrent: succeeded");
	return True;
}


extern "C"
void glXDestroyContext(Display *display, GLXContext ctx)
{
	eglDestroyContext(display->dpy, ctx->context);
}


/*
 * Xlib
 */
extern "C" Window        XDefaultRootWindow(Display *) TRACE(Window());
extern "C" Colormap      XCreateColormap(Display *, Window, Visual *, int)  TRACE(Colormap(1));
extern "C" XErrorHandler XSetErrorHandler(XErrorHandler) TRACE(XErrorHandler(nullptr));

extern "C" int           XFree(void *) STOP
extern "C" Status        XGetWindowAttributes(Display *, Window, XWindowAttributes *) STOP
extern "C" int           XMapWindow(Display *, Window) STOP
extern "C" int           XNextEvent(Display *, XEvent *) STOP
extern "C" int           XScreenNumberOfScreen(Screen *) STOP
extern "C" int           XSync(Display *, Bool) STOP


extern "C"
Display * XOpenDisplay(char *)
{
	Display *display = new Display();
	display->dpy = eglGetDisplay(EGLNativeDisplayType());
	return display;
}


extern "C"
int XCloseDisplay(Display *display)
{
	delete display;
	return 0;
}


extern "C"
int XPending(Display *)
{
	static bool once = true;

	if (once) {
		Genode::error(__func__, " called by 'vmsvga3dXEventThread' implement!");
		once = false;
	}

	return 0;
}


extern "C"
Window XCreateWindow(Display *display, Window,
                     int, int,
                     unsigned width, unsigned height,
                     unsigned int, int, unsigned int,
                     Visual *visual, unsigned long, XSetWindowAttributes *)
{
	Genode_egl_window *egl_window = new Genode_egl_window();
	egl_window->width  = width;
	egl_window->height = height;
	egl_window->type   = Surface_type::WINDOW;
	egl_window->addr   = (unsigned char*)0xcafebabe;

	Window window = new _Window();
	window->window = egl_window;

	window->surface = eglCreateWindowSurface(display->dpy, visual->config, egl_window, NULL);
	if (window->surface == EGL_NO_SURFACE) {
		Genode::error("could not create surface");
		return nullptr;
	}

	return window;
}


extern "C" int XDestroyWindow(Display *display, Window window)
{
	eglDestroySurface(display->dpy, window->surface);
	delete window->window;
	delete window;
	return 0;
}
