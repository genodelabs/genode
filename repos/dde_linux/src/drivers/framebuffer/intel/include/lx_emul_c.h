/*
 * \brief  C-declarations needed for device driver environment
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2016-06-17
 */

#ifndef _LX_EMUL_C_H_
#define _LX_EMUL_C_H_

#if 0
#define TRACE \
	do { \
		lx_printf("%s not implemented\n", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		BUG(); \
	} while (0)

#define ASSERT(x) \
	do { \
		if (!(x)) { \
			lx_printf("%s:%u assertion failed\n", __func__, __LINE__); \
			BUG(); \
		} \
	} while (0)

#include <lx_emul/extern_c_begin.h>

struct drm_device;
struct drm_framebuffer;
struct drm_display_mode;
struct drm_connector;

struct lx_c_fb_config {
	int                      height;
	int                      width;
	unsigned                 pitch;
	unsigned                 bpp;
	void                   * addr;
	unsigned long            size;
	struct drm_framebuffer * lx_fb;
};

void   lx_c_allocate_framebuffer(struct drm_device *,
                                 struct lx_c_fb_config *);
void   lx_c_set_mode(struct drm_device *, struct drm_connector *,
                     struct drm_framebuffer *, struct drm_display_mode *);
void   lx_c_set_driver(struct drm_device *, void *);
void * lx_c_get_driver(struct drm_device *);

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_C_H_ */
