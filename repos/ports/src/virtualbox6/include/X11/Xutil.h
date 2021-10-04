#ifndef _X11__XUTIL_H_
#define _X11__XUTIL_H_

#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	Visual *visual;
	VisualID visualid;
	int screen;
	int depth;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

#ifdef __cplusplus
}
#endif

#endif /* _X11__XUTIL_H_ */

