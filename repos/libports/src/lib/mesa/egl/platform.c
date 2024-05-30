/**
 * \brief  Generic EGL-DRI2 back end
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Mesa
 */
#include <egl_dri2.h>
#include <util/xmlconfig.h>

/*
 * Libc
 */
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

EGLBoolean dri2_genode_swap_interval(_EGLDisplay *disp,
                                     _EGLSurface *surf, EGLint interval)
{
	if (interval > surf->Config->MaxSwapInterval)
		interval = surf->Config->MaxSwapInterval;
	else if (interval < surf->Config->MinSwapInterval)
		interval = surf->Config->MinSwapInterval;

	surf->SwapInterval = interval;

	return EGL_TRUE;
}


static _EGLSurface *
_create_surface(_EGLDisplay *disp,
                _EGLConfig *conf, void *native_window,
                const EGLint *attrib_list,
                enum Surface_type type)
{
	struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
	struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
	struct Genode_egl_window  *window = native_window;
	struct Genode_egl_window  *window_dynamic;
	struct dri2_egl_surface *dri2_surf;
	const __DRIconfig *config;

	window->type = type;
	dri2_surf = calloc(1, sizeof *dri2_surf);

	if (type == PIXMAP) {
		window_dynamic = calloc(1, sizeof(struct Genode_egl_window));
		*window_dynamic = *window;
		 window = window_dynamic;
	}

	if (!dri2_surf)
	{
		_eglError(EGL_BAD_ALLOC, "dri2_create_surface");
		return NULL;
	}

	if (!_eglInitSurface(&dri2_surf->base, disp, EGL_WINDOW_BIT, conf, attrib_list, native_window))
	   goto cleanup_surf;

	dri2_surf->g_win = window;
	dri2_surf->base.Width  = window->width;;
	dri2_surf->base.Height = window->height;

	config = dri2_get_dri_config(dri2_conf, EGL_WINDOW_BIT,
	                             dri2_surf->base.GLColorspace);

	if (dri2_dpy->dri2) {
		dri2_surf->dri_drawable = (*dri2_dpy->dri2->createNewDrawable)(dri2_dpy->dri_screen_render_gpu, config,
		                                                               dri2_surf);
		/* create back buffer image */
		unsigned flags = 0;
		flags |= __DRI_IMAGE_USE_LINEAR;
		flags |= (__DRI_IMAGE_USE_SHARE | __DRI_IMAGE_USE_BACKBUFFER);
		dri2_surf->back_image = dri2_dpy->image->createImage(dri2_dpy->dri_screen_render_gpu,
		                                                     dri2_surf->base.Width,
		                                                     dri2_surf->base.Height,
		                                                     __DRI_IMAGE_FORMAT_XRGB8888,
		                                                     flags,
		                                                     NULL);
	} else {
		assert(dri2_dpy->swrast);
		dri2_surf->dri_drawable =
		   (*dri2_dpy->swrast->createNewDrawable)(dri2_dpy->dri_screen_render_gpu,
		                                          config, dri2_surf);
	}

	if (dri2_surf->dri_drawable == NULL)
	{
		_eglError(EGL_BAD_ALLOC, "swrast->createNewDrawable");
		 goto cleanup_dri_drawable;
	}

	dri2_genode_swap_interval(disp, &dri2_surf->base,
	                          dri2_dpy->default_swap_interval);

	return &dri2_surf->base;

cleanup_dri_drawable:
	dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
cleanup_surf:
	if (type == PIXMAP)
		free(window_dynamic);
	free(dri2_surf);

	return NULL;
}


_EGLSurface *
dri2_genode_create_window_surface(_EGLDisplay *disp,
                                  _EGLConfig *conf, void *native_window,
                                  const EGLint *attrib_list)
{
	_EGLSurface *surf = _create_surface(disp, conf, native_window, attrib_list, WINDOW);
	return surf;
}


_EGLSurface*
dri2_genode_create_pixmap_surface(_EGLDisplay *dpy,
                                  _EGLConfig *conf, void *native_pixmap,
                                  const EGLint *attrib_list)
{
	return _create_surface(dpy, conf, native_pixmap, attrib_list, PIXMAP);
}


EGLBoolean
dri2_genode_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
	struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
	struct dri2_egl_display *dri2_dpy  = dri2_egl_display(disp);
	struct Genode_egl_window *window   = dri2_surf->g_win;

	dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

	if (dri2_surf->back_image)
		dri2_dpy->image->destroyImage(dri2_surf->back_image);

	if (window->type == PIXMAP)
		free(window);

	free(dri2_surf);

	return EGL_TRUE;
}



EGLBoolean dri2_initialize_genode(_EGLDisplay *disp)
{
	void *handle;

	if (!(handle = dlopen("mesa_gpu.lib.so", 0))) {
		printf("Error: could not open EGL back end driver ('mesa_gpu.lib.so')\n");
		return EGL_FALSE;
	}

	/*
	 * xmlconfig.c expects a valid 'execname' variable (see file). Since
	 * the fallback 'getprogname' returns NULL, inject something
	 */
	driInjectExecName("mesa_app");

	typedef EGLBoolean (*genode_backend)(_EGLDisplay *);

	genode_backend init = (genode_backend)dlsym(handle, "dri2_initialize_genode_backend");
	if (!init) {
		printf("Error: could not find 'dri2_initialize_genode_backend'\n");
		return EGL_FALSE;
	}

	return init(disp);
}

EGLBoolean
dri2_initialize_surfaceless(_EGLDisplay *disp)
{
	printf("%s:%d\n", __func__, __LINE__);
	while (1) ;
	return false;
}
