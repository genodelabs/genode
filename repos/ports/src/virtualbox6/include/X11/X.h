#ifndef _X11__X_H_
#define _X11__X_H_

#ifdef __cplusplus
extern "C" {
#endif

/* originally in X11/Xmd.h */
typedef unsigned int CARD32;

typedef CARD32 XID;
typedef CARD32 VisualID;
typedef XID    Window;
typedef XID    Colormap;

#define None        0L
#define InputOutput 1
#define AllocNone   0

#define StructureNotifyMask (1L<<17)

#define CWBackPixel        (1L<<1)
#define CWBorderPixel      (1L<<3)
#define CWOverrideRedirect (1L<<9)
#define CWEventMask        (1L<<11)
#define CWColormap         (1L<<13)

#ifdef __cplusplus
}
#endif

#endif /* _X11__X_H_ */
