/**
 * \brief  Intel GPU EGL-DRI2 back end
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
#include <egl_dri2_fallbacks.h>
#include <drivers/dri/common/utils.h>

/*
 * Libc
 */
#include <dlfcn.h>

/*
 * Local
 */
#include <bo_map.h>
#include <platform.h>


static int stride(int value)
{
	/* RGB556 */
	return value * 2;
}

typedef void *(*mem_copy_fn)(void *dest, const void *src, size_t n);
extern void
tiled_to_linear(uint32_t xt1, uint32_t xt2,
                uint32_t yt1, uint32_t yt2,
                char *dst, const char *src,
                int32_t dst_pitch, uint32_t src_pitch,
                bool has_swizzling,
                uint32_t tiling,
                mem_copy_fn mem_copy);


static void
dri2_genode_put_image(__DRIdrawable * draw, int op,
                      int x, int y, int w, int h,
                      char *data, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf  = loaderPrivate;
	struct dri2_egl_display *dri2_dpy   = dri2_egl_display(dri2_surf->base.Resource.Display);
	struct Genode_egl_window  *window   = dri2_surf->g_win;
	unsigned char *dst                 = window->addr;

	int src_stride;
	int dst_stride = stride(dri2_surf->base.Width);
	dri2_dpy->image->queryImage(dri2_surf->back_image, __DRI_IMAGE_ATTRIB_STRIDE, &src_stride);

	/* copy to frame buffer and refresh */
	tiled_to_linear(0, dst_stride,
	                0, h,
	                (char *)dst, data,
	                dst_stride, src_stride,
	                false, 1, memcpy);
}


static EGLBoolean
dri2_genode_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *draw)
{
	struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

	genode_drm_complete();

	void *data = genode_map_image(dri2_surf->back_image);
	dri2_genode_put_image(dri2_surf->dri_drawable, 0, 0, 0,
	                      dri2_surf->base.Width, dri2_surf->base.Height,
	                      (char *)data, (void *)dri2_surf);

	genode_unmap_image(dri2_surf->back_image);

	return EGL_TRUE;
}


/*
 * platform functions
 */
static struct dri2_egl_display_vtbl dri2_genode_display_vtbl = {
	.authenticate = NULL,
	.create_window_surface = dri2_genode_create_window_surface,
	.create_pixmap_surface = dri2_genode_create_pixmap_surface,
	.create_pbuffer_surface = dri2_fallback_create_pbuffer_surface,
	.destroy_surface = dri2_genode_destroy_surface,
	.create_image = dri2_fallback_create_image_khr,
	.swap_interval = dri2_genode_swap_interval,
	.swap_buffers = dri2_genode_swap_buffers,
	.swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
	.swap_buffers_region = dri2_fallback_swap_buffers_region,
	.post_sub_buffer = dri2_fallback_post_sub_buffer,
	.copy_buffers = dri2_fallback_copy_buffers,
	.query_buffer_age = dri2_fallback_query_buffer_age,
	.get_sync_values = dri2_fallback_get_sync_values,
	.get_dri_drawable = dri2_surface_get_dri_drawable,
};


static __DRIbuffer *
dri2_genode_get_buffers(__DRIdrawable * driDrawable,
                        int *width, int *height,
                        unsigned int *attachments, int count,
                        int *out_count, void *loaderPrivate)
{
	_eglError(EGL_BAD_PARAMETER, "dri2_genode_get_buffers not implemented");
	return NULL;
}


static void
dri2_genode_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
	_eglError(EGL_BAD_PARAMETER, "dri2_genode_flush_front_buffer not implemented");
}


static void
back_bo_to_dri_buffer(struct dri2_egl_surface *dri2_surf, __DRIbuffer *buffer)
{
	struct dri2_egl_display *dri2_dpy = dri2_egl_display(dri2_surf->base.Resource.Display);
	__DRIimage *image;
	int name, pitch;

	image = dri2_surf->back_image;

	dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_NAME, &name);
	dri2_dpy->image->queryImage(image, __DRI_IMAGE_ATTRIB_STRIDE, &pitch);

	buffer->attachment = __DRI_BUFFER_BACK_LEFT;
	buffer->name = name;
	buffer->pitch = pitch;
	buffer->cpp = 4;
	buffer->flags = 0;
}


static __DRIbuffer *
dri2_genode_get_buffers_with_format(__DRIdrawable * driDrawable,
                                    int *width, int *height,
                                    unsigned int *attachments, int count,
                                    int *out_count, void *loaderPrivate)
{
	struct dri2_egl_surface *dri2_surf = loaderPrivate;
	int i, j;

	for (i = 0, j = 0; i < 2 * count; i += 2, j++) {
		switch (attachments[i]) {
		case __DRI_BUFFER_BACK_LEFT:
			back_bo_to_dri_buffer(dri2_surf, &dri2_surf->buffers[j]);
			break;
		default:
//			if (get_aux_bo(dri2_surf, attachments[i], attachments[i + 1],
//			               &dri2_surf->buffers[j]) < 0) {
//				_eglError(EGL_BAD_ALLOC, "failed to allocate aux buffer");
//				return NULL;
//			}
			printf("ERROR: not implemented\n");
			while (1);
			break;
		}
	}

	*out_count = j;
	if (j == 0)
		return NULL;

	*width = dri2_surf->base.Width;
	*height = dri2_surf->base.Height;

	return dri2_surf->buffers;
}


EGLBoolean
dri2_initialize_genode_backend(_EGLDriver *drv, _EGLDisplay *disp)
{
	struct dri2_egl_display *dri2_dpy;
	static unsigned rgb565_masks[4] = { 0xf800, 0x07e0, 0x001f, 0 };
	int i;

	/* initialize DRM back end */
	genode_drm_init();

	dri2_dpy = calloc(1, sizeof *dri2_dpy);
	if (!dri2_dpy)
		return _eglError(EGL_BAD_ALLOC, "eglInitialize");

	dri2_dpy->fd          = -1;
	dri2_dpy->driver_name = strdup("i965");

	disp->DriverData = (void *)dri2_dpy;
	dri2_dpy->vtbl   = &dri2_genode_display_vtbl;

	if (!dri2_load_driver(disp))
		goto cleanup_dpy;

	dri2_dpy->dri2_major = 2;
	dri2_dpy->dri2_minor = __DRI_DRI2_VERSION;
	dri2_dpy->dri2_loader_extension.base.name = __DRI_DRI2_LOADER;
	dri2_dpy->dri2_loader_extension.base.version = 3;
	dri2_dpy->dri2_loader_extension.getBuffers = dri2_genode_get_buffers;
	dri2_dpy->dri2_loader_extension.flushFrontBuffer = dri2_genode_flush_front_buffer;
	dri2_dpy->dri2_loader_extension.getBuffersWithFormat = dri2_genode_get_buffers_with_format;


	dri2_dpy->extensions[0] = &dri2_dpy->dri2_loader_extension.base;
	dri2_dpy->extensions[1] = &image_lookup_extension.base;
	dri2_dpy->extensions[2] = NULL;
	
	dri2_dpy->swap_available = (dri2_dpy->dri2_minor >= 2);
	dri2_dpy->invalidate_available = (dri2_dpy->dri2_minor >= 3);

	if (!dri2_create_screen(disp))
		goto close_screen;


	/* add RGB565 only */
	EGLint attrs[] = {
		EGL_DEPTH_SIZE, 0, /* set in loop below (from DRI config) */
		EGL_NATIVE_VISUAL_TYPE, 0,
		EGL_NATIVE_VISUAL_ID, 0,
		EGL_RED_SIZE, 5,
		EGL_GREEN_SIZE, 6,
		EGL_BLUE_SIZE, 5,
		EGL_NONE };

	for (i = 1; dri2_dpy->driver_configs[i]; i++) {
		/* set depth size in attrs */
		attrs[1] = dri2_dpy->driver_configs[i]->modes.depthBits;
		dri2_add_config(disp, dri2_dpy->driver_configs[i], i, EGL_WINDOW_BIT, attrs, rgb565_masks);
	}

	return EGL_TRUE;

close_screen:
	dlclose(dri2_dpy->driver);
cleanup_dpy:
	free(dri2_dpy);

	return EGL_FALSE;
}
