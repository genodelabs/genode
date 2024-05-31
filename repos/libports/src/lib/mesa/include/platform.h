/**
 * \brief  Platform C/C++ inteface
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef __cplusplus
namespace Genode { class Env; }
extern Genode::Env *genode_env;
#endif

#include <EGL/egl.h>

enum {
	ETNAVIV_FD = 10042,
	IRIS_FD    = 10043,
	LIMA_FD    = 10044
};

struct Genode_egl_window;
void genode_blit(void const *src, unsigned src_w, void *dst, unsigned dst_w, int w, int h);
void genode_drm_init();
void genode_drm_complete();

EGLSurface *
dri2_genode_create_window_surface(EGLDisplay *disp,
                                  EGLConfig *conf, void *native_window,
                                  const EGLint *attrib_list);
EGLBoolean
dri2_genode_destroy_surface(EGLDisplay *disp, EGLSurface *surf);

EGLSurface *
dri2_genode_create_pixmap_surface(EGLDisplay *disp,
                                  EGLConfig *conf, void *native_window,
                                  const EGLint *attrib_list);

EGLBoolean
dri2_genode_swap_interval(EGLDisplay *disp,
                          EGLSurface *surf, EGLint interval);

#endif
