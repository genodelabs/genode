#ifndef _X11__XLIB_H_
#define _X11__XLIB_H_

#include <X11/X.h>
#include <EGL/egl.h>
#ifdef __cplusplus
extern "C" {
#endif

#define Bool   int
#define Status int
#define True   1
#define False  0

typedef struct { EGLDisplay dpy; } Display;
typedef struct { void *dummy; } Screen;
typedef struct
{
	EGLConfig           config;
} Visual;

struct _Window
{
	EGLSurface          surface;
	EGLNativeWindowType window;
};

typedef struct { unsigned char error_code; } XErrorEvent;

typedef union _XEvent {
	int         type;
	XErrorEvent xerror;
} XEvent;

typedef struct { Screen *screen; } XWindowAttributes;

typedef struct {
	unsigned long background_pixel;
	unsigned long border_pixel;
	long event_mask;
	Bool override_redirect;
	Colormap colormap;
} XSetWindowAttributes;

typedef int (*XErrorHandler) (Display *, XErrorEvent *);

extern int           XCloseDisplay(Display *);
extern Colormap      XCreateColormap(Display *, Window, Visual *, int);
extern Window        XCreateWindow( Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *);
extern Window        XDefaultRootWindow(Display *);
extern int           XDestroyWindow(Display *, Window);
extern XErrorHandler XSetErrorHandler(XErrorHandler);
extern int           XFree(void *);
extern Status        XGetWindowAttributes(Display *, Window, XWindowAttributes *);
extern int           XMapWindow(Display *, Window);
extern int           XNextEvent(Display *, XEvent *);
extern Display *     XOpenDisplay(char *name);
extern int           XPending(Display *);
extern int           XScreenNumberOfScreen(Screen *);
extern int           XSync(Display *, Bool);

#define DefaultScreen(display) (0)

#ifdef __cplusplus
}
#endif

#endif /* _X11__XLIB_H_ */
