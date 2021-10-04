/*
 * \brief  GLX/X11 emulation for SVGA3D
 * \author Christian Helmuth
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

static bool const debug = true;

using namespace Genode;


/* from VBoxSVGA3DLazyLoad.asm */
extern "C" int ExplicitlyLoadVBoxSVGA3D(bool fResolveAllImports, PRTERRINFO pErrInfo) TRACE(VINF_SUCCESS);


/*
 * GLX
 */

//static void impl_glXGetProcAddress(GLubyte const *procname)
//{
//}

void (*glXGetProcAddress(const GLubyte *procname))(void)
{
	log(__func__, ": procname='", (char const *)procname, "'");

	return (void(*)())nullptr;
}

extern "C" GLXFBConfig * glXChooseFBConfig(Display *dpy, int screen, const int *attribList, int *nitems) STOP
extern "C" XVisualInfo * glXChooseVisual(Display *dpy, int screen, int *attribList) STOP
extern "C" GLXContext    glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct) STOP
extern "C" void          glXDestroyContext(Display *dpy, GLXContext ctx) STOP
extern "C" int           glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value) STOP
extern "C" XVisualInfo * glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config) STOP
extern "C" Bool          glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) STOP
extern "C" Bool          glXQueryVersion(Display *dpy, int *maj, int *min) STOP


/*
 * Xlib
 */

extern "C" int           XCloseDisplay(Display *)                              STOP
extern "C" Colormap      XCreateColormap(Display *, Window, Visual *, int)                              STOP
extern "C" Window        XCreateWindow( Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *) STOP
extern "C" Window        XDefaultRootWindow(Display *)                              STOP
extern "C" int           XDestroyWindow(Display *, Window)                              STOP
extern "C" XErrorHandler XSetErrorHandler(XErrorHandler)                              STOP
extern "C" int           XFree(void *)                              STOP
extern "C" Status        XGetWindowAttributes(Display *, Window, XWindowAttributes *)                              STOP
extern "C" int           XMapWindow(Display *, Window)                              STOP
extern "C" int           XNextEvent(Display *, XEvent *)                              STOP
extern "C" Display *     XOpenDisplay(char *name)                              STOP
extern "C" int           XPending(Display *)                              STOP
extern "C" int           XScreenNumberOfScreen(Screen *)                              STOP
extern "C" int           XSync(Display *, Bool)                              STOP
