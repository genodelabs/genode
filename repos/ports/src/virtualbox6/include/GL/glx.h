#ifndef _GL__GLX_H_
#define _GL__GLX_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>


#ifdef __cplusplus
extern "C" {
#endif


#define GLX_RGBA                4
#define GLX_DOUBLEBUFFER        5
#define GLX_RED_SIZE            8
#define GLX_GREEN_SIZE          9
#define GLX_BLUE_SIZE           10
#define GLX_ALPHA_SIZE          11
#define GLX_DEPTH_SIZE          12
#define GLX_STENCIL_SIZE        13

#define GLX_WINDOW_BIT          0x00000001
#define GLX_DRAWABLE_TYPE       0x8010

typedef struct _GLXContext  { void *dummy; } * GLXContext;
typedef struct _GLXFBConfig { void *dummy; } * GLXFBConfig;

typedef XID GLXDrawable;

extern void (*glXGetProcAddress(const GLubyte *procname))(void);

extern GLXFBConfig * glXChooseFBConfig(Display *dpy, int screen, const int *attribList, int *nitems);
extern XVisualInfo * glXChooseVisual(Display *dpy, int screen, int *attribList);
extern GLXContext    glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
extern void          glXDestroyContext(Display *dpy, GLXContext ctx);
extern int           glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value);
extern XVisualInfo * glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config);
extern Bool          glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
extern Bool          glXQueryVersion(Display *dpy, int *maj, int *min);


#ifdef __cplusplus
}
#endif

#endif /* _GL__GLX_H_ */
