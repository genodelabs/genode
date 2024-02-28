/**
 * \brief  Software EGL-DRI2 back end
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
#include <gallium/frontends/dri/dri_util.h>

/*
 * Libc
 */
#include <string.h>
#include <dlfcn.h>

/*
 * Local
 */
#include <platform.h>

static int stride(int value)
{
	/* 32-bit RGB888 */
	return value * 4;
}


static EGLBoolean
dri2_genode_swrast_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
	struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
	struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

	dri2_dpy->core->swapBuffers(dri2_surf->dri_drawable);
	return EGL_TRUE;
}


static struct dri2_egl_display_vtbl dri2_genode_display_vtbl = {
	.authenticate = NULL,
	.create_window_surface = dri2_genode_create_window_surface,
	.create_pixmap_surface = dri2_genode_create_pixmap_surface,
	//.create_pbuffer_surface = dri2_fallback_create_pbuffer_surface,
	.destroy_surface = dri2_genode_destroy_surface,
	//.create_image = dri2_fallback_create_image_khr,
	.swap_interval = dri2_genode_swap_interval,
	.swap_buffers = dri2_genode_swrast_swap_buffers,
	//.swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
	//.swap_buffers_region = dri2_fallback_swap_buffers_region,
	//.post_sub_buffer = dri2_fallback_post_sub_buffer,
	//.copy_buffers = dri2_fallback_copy_buffers,
	//.query_buffer_age = dri2_fallback_query_buffer_age,
	//.get_sync_values = dri2_fallback_get_sync_values,
	.get_dri_drawable = dri2_surface_get_dri_drawable,
};


static void
dri2_genode_swrast_get_image(__DRIdrawable * read,
                             int x, int y, int w, int h,
                             char *data, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf  = loaderPrivate;
	struct Genode_egl_window  *window   = dri2_surf->g_win;
	unsigned char * src                 = window->addr;

	int src_stride = stride(dri2_surf->base.Width);
	int copy_width = stride(w);
	int x_offset = stride(x);
	int dst_stride = copy_width;

	assert(data != (char *)src);

	src += x_offset;
	src += y * src_stride;

	/* copy width over stride boundary */
	if (copy_width > src_stride - x_offset)
		copy_width = src_stride - x_offset;

	/* limit height */
	if (h > dri2_surf->base.Height - y)
		h = dri2_surf->base.Height - y;

	/* copy to surface */
	genode_blit(src, src_stride, data, dst_stride, copy_width, h);
}


static void
dri2_genode_swrast_put_image(__DRIdrawable * draw, int op,
                             int x, int y, int w, int h,
                             char *data, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf  = loaderPrivate;
	struct Genode_egl_window  *window   = dri2_surf->g_win;
	unsigned char * dst                 = window->addr;

	int dst_stride = stride(dri2_surf->base.Width);
	int copy_width = stride(w);
	int x_offset = stride(x);
	int src_stride = copy_width;

	dst += x_offset;
	dst += y * dst_stride;

	/* copy width over stride boundary */
	if (copy_width >dst_stride - x_offset)
		copy_width = dst_stride - x_offset;

	/* limit height */
	if (h > dri2_surf->base.Height - y)
		h = dri2_surf->base.Height - y;

	/* copy to frame buffer and refresh */
	genode_blit(data, src_stride, dst, dst_stride, copy_width, h);
}


static void
dri2_genode_swrast_get_drawable_info(__DRIdrawable * draw,
                                 int *x, int *y, int *w, int *h,
                                 void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf = loaderPrivate;

	//XXX: (void) swrast_update_buffers(dri2_surf);
	struct Genode_egl_window  *window   = dri2_surf->g_win;

	*x = 0;
	*y = 0;
	*w = window->width;
	*h = window->height;

	dri2_surf->base.Width  = window->width;
	dri2_surf->base.Height = window->height;

}


static const __DRIswrastLoaderExtension swrast_loader_extension = {
	.base = { __DRI_SWRAST_LOADER, 1 },

	.getDrawableInfo = dri2_genode_swrast_get_drawable_info,
	.putImage        = dri2_genode_swrast_put_image,
	.getImage        = dri2_genode_swrast_get_image
};

static const __DRIextension *swrast_loader_extensions[] = {
	&swrast_loader_extension.base,
	NULL,
	NULL,
};


static EGLBoolean
dri2_initialize_genode_swrast(_EGLDisplay *disp)
{
	struct dri2_egl_display *dri2_dpy;
	static int      rgb888_shifts[4] = { 16, 8, 0, 24 };
	static unsigned rgb888_sizes[4]  = {  8, 8, 8, 8 };
	int i;

	dri2_dpy = calloc(1, sizeof *dri2_dpy);

	if (!dri2_dpy)
		return _eglError(EGL_BAD_ALLOC, "eglInitialize");

	disp->DriverData = (void *)dri2_dpy;

	dri2_dpy->fd_render_gpu = -1;
	dri2_dpy->driver_name = strdup("swrast");
	if (!dri2_load_driver_swrast(disp))
		goto close_driver;

	dri2_dpy->loader_extensions = swrast_loader_extensions;

	if (!dri2_create_screen(disp))
		goto close_screen;

	if (!dri2_setup_extensions(disp))
		goto close_screen;

	dri2_setup_screen(disp);

	EGLint attrs[] = {
		EGL_DEPTH_SIZE, 0, /* set in loop below (from DRI config) */
		EGL_NATIVE_VISUAL_TYPE, 0,
		EGL_NATIVE_VISUAL_ID, 0,
		EGL_NONE };

	for (i = 0; dri2_dpy->driver_configs[i]; i++) {
		/* set depth size in attrs */
		attrs[1] = dri2_dpy->driver_configs[i]->modes.depthBits;
		dri2_add_config(disp, dri2_dpy->driver_configs[i], i,
		                EGL_WINDOW_BIT | EGL_PIXMAP_BIT | EGL_PBUFFER_BIT,
		                attrs, rgb888_shifts, rgb888_sizes);
	}

	dri2_dpy->vtbl   = &dri2_genode_display_vtbl;

	return EGL_TRUE;

close_screen:
	dlclose(dri2_dpy->driver);
close_driver:
	free(dri2_dpy);

	return EGL_FALSE;
}


EGLBoolean dri2_initialize_genode_backend(_EGLDisplay *disp)
{
	return  dri2_initialize_genode_swrast(disp);
}
