/*
 * \brief  Gallium EGL driver for Genode
 * \author Norman Feske
 * \date   2010-07-05
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/snprintf.h>
#include <base/env.h>
#include <util/misc_math.h>  /* for 'max' */
#include <framebuffer_session/connection.h>
#include <timer_session/connection.h>
#include <blit/blit.h>

/* libc includes */
#include <dlfcn.h> /* for 'dlopen', 'dlsym', and the like */
#include <fcntl.h> /* for 'open' */

/* includes from 'egl/main' */
extern "C" {
#include <egllog.h>
}

/* includes from 'gallium/state_trackers/egl' */
extern "C" {
#include <common/native.h>
#include <common/native_modeset.h>
}

/* Gallium includes */
extern "C" {
#include <auxiliary/util/u_simple_screen.h> /* for 'struct pipe_winsys' */
#include <auxiliary/util/u_inlines.h>       /* for 'pipe_reference_init' */
#include <auxiliary/util/u_memory.h>        /* for 'align_malloc' */
#include <auxiliary/util/u_format.h>        /* for 'util_format_get_nblocksy' */
#include <auxiliary/util/u_math.h>          /* for 'align' */
#include <drivers/softpipe/sp_winsys.h>     /* for 'softpipe_create_screen' */
#include <state_tracker/drm_api.h>          /* for 'drm_api_create' */
}

/* local includes */
#include "select_driver.h"


static bool do_clflush = true;


/********************************
 ** Genode framebuffer backend **
 ********************************/

class Genode_framebuffer
{
	private:

		Framebuffer::Connection            _framebuffer;
		Framebuffer::Mode            const _mode;
		Genode::Dataspace_capability const _ds_cap;
		void                       * const _local_addr;

	public:

		Genode_framebuffer()
		:
			_mode(_framebuffer.mode()),
			_ds_cap(_framebuffer.dataspace()),
			_local_addr(Genode::env()->rm_session()->attach(_ds_cap))
		{ }

		~Genode_framebuffer()
		{
			Genode::env()->rm_session()->detach(_local_addr);
		}

		void *local_addr() const { return _local_addr; }

		void flush()
		{
			_framebuffer.refresh(0, 0, _mode.width(), _mode.height());
		}

		int width()  const { return _mode.width();  }
		int height() const { return _mode.height(); }
};


static Genode_framebuffer *genode_framebuffer()
{
	static Genode_framebuffer genode_framebuffer_inst;
	return &genode_framebuffer_inst;
}


/************
 ** Winsys **
 ************/

class Pipe_buffer : public pipe_buffer
{
	void *_data;

	public:

		/**
		 * Constructor
		 */
		Pipe_buffer(unsigned alignment,
		            unsigned usage,
		            unsigned size)
		{
			pipe_reference_init(&reference, 1);
			pipe_buffer::alignment = alignment;
			pipe_buffer::usage     = usage;
			pipe_buffer::size      = size;

			/* align to 16-byte multiple for Cell */
			_data = align_malloc(size, Genode::max(alignment, 16U));
		}

		/**
		 * Destructor
		 */
		~Pipe_buffer() { align_free(_data); }

		void *data() const { return _data; }
};


class Winsys : public pipe_winsys
{
	/*
	 * For documentation of the winsys functions, refer to
	 * 'auxiliary/util/u_simple_screen.h'.
	 */

	static void
	_destroy(struct pipe_winsys *ws)
	{
		Genode::destroy(Genode::env()->heap(), static_cast<Winsys *>(ws));
	}

	static const char *
	_get_name(struct pipe_winsys *ws)
	{
		return "Genode-winsys";
	}

	static void
	_update_buffer(struct pipe_winsys *ws,
	               void *context_private)
	{
		Genode::warning(__func__, " not implemented");
	}

	static void
	_flush_frontbuffer(struct pipe_winsys *ws,
	                   struct pipe_surface *surf,
	                   void *context_private)
	{
		genode_framebuffer()->flush();
	}

	static struct pipe_buffer *
	_buffer_create(struct pipe_winsys *ws,
	               unsigned alignment,
	               unsigned usage,
	               unsigned size)
	{
		return new (Genode::env()->heap()) Pipe_buffer(alignment, usage, size);
	}

	static struct pipe_buffer *
	_user_buffer_create(struct pipe_winsys *ws,
	                    void *ptr,
	                    unsigned bytes)
	{
		Pipe_buffer *buf =
			new (Genode::env()->heap()) Pipe_buffer(64, 0, bytes);
		Genode::memcpy(buf->data(), ptr, bytes);
		return buf;
	}

	/*
	 * Called when using the softpipe driver
	 */
	static struct pipe_buffer *
	_surface_buffer_create(struct pipe_winsys *winsys,
	                       unsigned width, unsigned height,
	                       enum pipe_format format,
	                       unsigned usage,
	                       unsigned tex_usage,
	                       unsigned *stride)
	{
		Genode::log("Winsys::_surface_buffer_create: "
		            "format=",    (int)format,  ", "
		            "stride=",    *stride,      ", "
		            "usage=",     usage,        ", "
		            "tex_usage=", Genode::Hex(tex_usage));

		unsigned nblocksy = util_format_get_nblocksy(format, height);

		enum { ALIGNMENT = 64 };
		*stride = align(util_format_get_stride(format, width), ALIGNMENT);

		return new (Genode::env()->heap()) Pipe_buffer(ALIGNMENT, usage, *stride * nblocksy);
	}

	static void *
	_buffer_map(struct pipe_winsys *ws,
	            struct pipe_buffer *buf,
	            unsigned usage)
	{
		return buf ? static_cast<Pipe_buffer *>(buf)->data() : 0;
	}

	static void
	_buffer_unmap(struct pipe_winsys *ws,
	              struct pipe_buffer *buf)
	{
	}

	static void
	_buffer_destroy(struct pipe_buffer *buf)
	{
		Genode::destroy(Genode::env()->heap(), static_cast<Pipe_buffer *>(buf));
	}

	static void
	_fence_reference(struct pipe_winsys *ws,
	                 struct pipe_fence_handle **ptr,
	                 struct pipe_fence_handle *fence)
	{
		Genode::warning(__func__, " not implemented");
	}

	static int
	_fence_signalled(struct pipe_winsys *ws,
	                 struct pipe_fence_handle *fence,
	                 unsigned flag )
	{
		Genode::warning(__func__, " not implemented"); return 0;
	}

	static int
	_fence_finish(struct pipe_winsys *ws,
	              struct pipe_fence_handle *fence,
	              unsigned flag )
	{
		Genode::warning(__func__, " not implemented"); return 0;
	}

	public:

		/**
		 * Constructor
		 */
		Winsys()
		{
			/* initialize members of 'struct pipe_winsys' */
			destroy               = _destroy;
			get_name              = _get_name;
			update_buffer         = _update_buffer;
			flush_frontbuffer     = _flush_frontbuffer;
			buffer_create         = _buffer_create;
			user_buffer_create    = _user_buffer_create;
			surface_buffer_create = _surface_buffer_create;
			buffer_map            = _buffer_map;
			buffer_unmap          = _buffer_unmap;
			buffer_destroy        = _buffer_destroy;
			fence_reference       = _fence_reference;
			fence_signalled       = _fence_signalled;
			fence_finish          = _fence_finish;
		}
};


/**************************
 ** EGL driver functions **
 **************************/

using namespace Genode;

/*
 * For documentation of the 'native_surface' and 'native_display' functions,
 * refer to 'gallium/state_trackers/egl/common/native.h'.
 * For documentation of the 'native_display_modeset' functions, refer to
 * 'gallium/state_trackers/egl/common/native_modeset.h'.
 */
class Display;

class Surface : public native_surface
{
	public:

		enum Surface_type { TYPE_SCANOUT, TYPE_WINDOW };

	private:

		enum pipe_format     _color_format;
		native_display      *_display;
		Surface_type         _type;
		const native_config *_config;
		int                  _width, _height;
		unsigned char       *_addr; /* only used for TYPE_WINDOW */
		struct pipe_texture *_textures[NUM_NATIVE_ATTACHMENTS];
		unsigned int         _sequence_number;

		static void
		_destroy(struct native_surface *nsurf)
		{
			Genode::destroy(Genode::env()->heap(), static_cast<Surface *>(nsurf));
		}

		static boolean
		_swap_buffers(struct native_surface *nsurf);

		static boolean
		_flush_frontbuffer(struct native_surface *nsurf)
		{
			Genode::warning(__func__, " not implemented"); return 0;
		}

		static boolean
		_validate(struct native_surface *nsurf, uint attachment_mask,
		          unsigned int *seq_num, struct pipe_texture **textures,
		          int *width, int *height)
		{
			Surface *this_surface = static_cast<Surface *>(nsurf);

			struct pipe_texture templ;
			if (attachment_mask) {
				::memset(&templ, 0, sizeof(templ));
				templ.target = PIPE_TEXTURE_2D;
				templ.last_level = 0;
				templ.width0 = this_surface->_width;
				templ.height0 = this_surface->_height;
				templ.depth0 = 1;
				templ.format = this_surface->_color_format;
				templ.tex_usage = PIPE_TEXTURE_USAGE_RENDER_TARGET;
				if (this_surface->_type == TYPE_SCANOUT)
					templ.tex_usage |= PIPE_TEXTURE_USAGE_PRIMARY;
			}

			/* create textures */
			for (int i = 0; i < NUM_NATIVE_ATTACHMENTS; i++) {

				/* delay the allocation */
				if (!native_attachment_mask_test(attachment_mask, (native_attachment)i))
					continue;

				struct pipe_texture *ptex = this_surface->_textures[i];
				if (!ptex) {

					struct pipe_screen *screen = this_surface->_display->screen;
					/*
					 * The softpipe driver implements 'texture_create' by
					 * calling 'surface_buffer_create' if the
					 * 'PIPE_TEXTURE_USAGE_PRIMARY' bit in tex_usage is set (see
					 * 'softpipe_texture_create').
					 * The call then ends up in 'Winsys::_surface_buffer_create'.
					 *
					 * The i915 driver, however, translates 'PIPE_TEXTURE_USAGE_PRIMARY'
					 * to an 'INTEL_NEW_SCANOUT' argument passed to 'buffer_create'
					 * (ending up in 'intel_drm_buffer_create'.
					 */
					ptex = screen->texture_create(screen, &templ);
					this_surface->_textures[i] = ptex;
				}

				if (textures) {
					textures[i] = 0;
					pipe_texture_reference(&textures[i], ptex);
				}
			}

			if (seq_num) *seq_num = this_surface->_sequence_number;
			if (width)   *width   = this_surface->_width;
			if (height)  *height  = this_surface->_height;

			return TRUE;
		}

		static void
		_wait(struct native_surface *nsurf)
		{
			Genode::warning(__func__, " not implemented");
		}

	public:

		Surface(native_display *display, Surface_type t, const native_config *config,
		        int width, int height, unsigned char *addr = 0)
		:
			_color_format(config->color_format), _display(display), _type(t),
			_config(config), _width(width), _height(height), _addr(addr),
			_sequence_number(0)
		{
			for (int i = 0; i < NUM_NATIVE_ATTACHMENTS; i++)
				_textures[i] = 0;

			/* setup members of 'struct native_surface' */
			destroy           = _destroy;
			swap_buffers      = _swap_buffers;
			flush_frontbuffer = _flush_frontbuffer;
			validate          = _validate;
			wait              = _wait;
		}

		~Surface()
		{
			for (int i = 0; i < NUM_NATIVE_ATTACHMENTS; i++)
				if (_textures[i])
					pipe_texture_reference(&_textures[i], 0);
		}

		/**
		 * Return texture used as backing store for the surface
		 */
		pipe_texture *texture()
		{
			for (int i = 0; i < NUM_NATIVE_ATTACHMENTS; i++)
				if (_textures[i])
					return _textures[i];
			return 0;
		}

		int width()  const { return _width; }
		int height() const { return _height; }
};


class Display : public native_display
{
	drm_api *_api;
	Winsys _winsys;

	enum { NUM_MODES = 1 };
	struct native_mode _mode;
	const struct native_mode *_mode_list[NUM_MODES];

	native_display_modeset _modeset;

	/*
	 * We only support one configuration.
	 */
	struct native_config _native_config;

	boolean
	_is_format_supported(enum pipe_format fmt, boolean is_color)
	{
		return screen->is_format_supported(screen,
			fmt, PIPE_TEXTURE_2D,
			(is_color) ? PIPE_TEXTURE_USAGE_RENDER_TARGET :
			PIPE_TEXTURE_USAGE_DEPTH_STENCIL, 0);
	}

	/***********************
	 ** Modeset functions **
	 ***********************/

	static const struct native_connector **
	_get_connectors(struct native_display *ndpy,
	                int *num_connectors, int *num_crtcs)
	{
		static native_connector conn;
		const native_connector **conn_list =
			(const native_connector **)malloc(sizeof(native_connector **));
		conn_list[0] = &conn;
		Genode::log("called, return 1 connector");

		if (num_connectors) *num_connectors = 1;
		if (num_crtcs)      *num_crtcs = 1;
		return conn_list;
	}

	static const struct native_mode **
	_get_modes(struct native_display *ndpy,
	           const struct native_connector *nconn,
	           int *num_modes)
	{
		*num_modes = 1;
		return static_cast<Display *>(ndpy)->_mode_list;
	}

	static struct native_surface *
	_create_scanout_surface(struct native_display *ndpy,
	                        const struct native_config *nconf,
	                        uint width, uint height)
	{
		return new (Genode::env()->heap())
			Surface(ndpy,
			        Surface::TYPE_SCANOUT, nconf,
			        width, height);
	}

	static boolean
	_program(struct native_display *ndpy, int crtc_idx,
	         struct native_surface *nsurf, uint x, uint y,
	         const struct native_connector **nconns, int num_nconns,
	         const struct native_mode *nmode)
	{
		return TRUE;
	}


	/***********************
	 ** Display functions **
	 ***********************/

	static void
	_destroy(struct native_display *ndpy)
	{
		Genode::destroy(Genode::env()->heap(), static_cast<Display *>(ndpy));
	}

	static int
	_get_param(struct native_display *ndpy, enum native_param_type param)
	{
		switch (param) {
		case NATIVE_PARAM_USE_NATIVE_BUFFER:
			return TRUE;
			break;
		default:
			return FALSE;
			break;
		}
	}

	static const struct native_config **
	_get_configs(struct native_display *ndpy, int *num_configs)
	{
		Display *display = static_cast<Display *>(ndpy);

		enum { NUM_CONFIGS = 1 };
		const struct native_config **configs =
			(const struct native_config **)calloc(1, sizeof(struct native_config *));
		configs[0] = &display->_native_config;

		struct native_config *config = &display->_native_config;
		config->mode.drawableType    = GLX_PBUFFER_BIT | GLX_WINDOW_BIT;

		int r = 5, g = 6, b = 5, a = 0;
		config->mode.swapMethod       = GLX_SWAP_EXCHANGE_OML;
		config->mode.visualID         = 0;
		config->mode.visualType       = EGL_NONE;
		config->mode.renderType       = GLX_RGBA_BIT;
		config->mode.rgbMode          = TRUE;
		config->mode.xRenderable      = FALSE;
		config->mode.maxPbufferWidth  = 4096;
		config->mode.maxPbufferHeight = 4096;
		config->mode.maxPbufferPixels = 4096*4096/256+3;
		config->mode.doubleBufferMode = TRUE;
		config->mode.rgbBits          = r + g + b + a;
		config->mode.redBits          = r;
		config->mode.greenBits        = g;
		config->mode.blueBits         = b;
		config->mode.alphaBits        = a;

		config->depth_format   = PIPE_FORMAT_NONE;
		config->stencil_format = PIPE_FORMAT_NONE;

		enum pipe_format format = PIPE_FORMAT_Z24S8_UNORM;
		if (!display->_is_format_supported(format, FALSE)) {
			format = PIPE_FORMAT_S8Z24_UNORM;
			if (!display->_is_format_supported(format, FALSE))
				format = PIPE_FORMAT_NONE;
		}

		if (format != PIPE_FORMAT_NONE) {
			Genode::log("support depth and stencil buffer");
			config->depth_format           = format;
			config->stencil_format         = format;
			config->mode.depthBits         = 24;
			config->mode.stencilBits       = 8;
			config->mode.haveDepthBuffer   = TRUE;
			config->mode.haveStencilBuffer = TRUE;
		}

		config->color_format = PIPE_FORMAT_B8G8R8A8_UNORM;
		config->color_format = PIPE_FORMAT_B5G6R5_UNORM;

		config->scanout_bit = TRUE;

		Genode::log("returning 1 config at ", config);

		*num_configs = NUM_CONFIGS;
		return configs;
	}

	static boolean
	_is_pixmap_supported(struct native_display *ndpy,
	                     EGLNativePixmapType pix,
	                     const struct native_config *nconf)
	{
		Genode::warning(__func__, " not implemented"); return 0;
	}

	static struct native_surface *
	_create_window_surface(struct native_display *ndpy,
	                       EGLNativeWindowType win,
	                       const struct native_config *nconf)
	{
		return new (Genode::env()->heap())
			Surface(ndpy,
			        Surface::TYPE_WINDOW, nconf,
			        win->width, win->height, win->addr);

	}

	static struct native_surface *
	_create_pixmap_surface(struct native_display *ndpy,
	                       EGLNativePixmapType pix,
	                       const struct native_config *nconf)
	{
		Genode::warning(__func__, " not implemented"); return 0;
	}

	static struct native_surface *
	_create_pbuffer_surface(struct native_display *ndpy,
	                        const struct native_config *nconf,
	                        uint width, uint height)
	{
		Genode::warning(__func__, " not implemented"); return 0;
	}

	public:

		/**
		 * Constructor
		 */
		Display(drm_api *api) : _api(api)
		{
			::memset(&_mode,          0, sizeof(_mode));
			::memset(&_modeset,       0, sizeof(_modeset));
			::memset(&_native_config, 0, sizeof(_native_config));

			/* setup mode list */
			_mode.desc         = "Mode-genode";
			try {
				_mode.width        = genode_framebuffer()->width();
				_mode.height       = genode_framebuffer()->height();
			}
			catch (Genode::Service_denied) {
				Genode::warning("EGL driver: could not create a Framebuffer session. "
				                "Screen surfaces cannot be used.");
				_mode.width  = 1;
				_mode.height = 1;
			}
			_mode.refresh_rate = 100;
			_mode_list[0]      = &_mode;

			/* setup members of 'struct native_display_modeset' */
			_modeset.get_connectors         = _get_connectors;
			_modeset.get_modes              = _get_modes;
			_modeset.create_scanout_surface = _create_scanout_surface;
			_modeset.program                = _program;

			/* setup members of 'struct native_display' */
			if (api) {
				struct drm_create_screen_arg arg;
				::memset(&arg, 0, sizeof(arg));
				arg.mode = DRM_CREATE_NORMAL;
				int drm_fd = open("/dev/drm", O_RDWR);
				screen = api->create_screen(api, drm_fd, &arg);
			} else {
				screen = softpipe_create_screen(&_winsys);
			}

			Genode::warning("returned from init display->screen");

			destroy                = _destroy;
			get_param              = _get_param;
			get_configs            = _get_configs;
			is_pixmap_supported    = _is_pixmap_supported;
			create_window_surface  = _create_window_surface;
			create_pixmap_surface  = _create_pixmap_surface;
			create_pbuffer_surface = _create_pbuffer_surface;
			modeset                = &_modeset;
		}
};


boolean Surface::_swap_buffers(struct native_surface *nsurf)
{
	Surface *this_surface = static_cast<Surface *>(nsurf);
	Display *display = static_cast<Display *>(this_surface->_display);
	pipe_screen *screen = display->screen;
	pipe_texture *texture = this_surface->texture();
	pipe_transfer *transfer = 0;

	static Timer::Connection timer;

	timer.msleep(5);

	if (!texture) {
		Genode::error("surface has no texture");
		return FALSE;
	}

	enum { FACE = 0, LEVEL = 0, ZSLICE = 0 };
	transfer = screen->get_tex_transfer(screen, texture, FACE, LEVEL, ZSLICE,
	                                    PIPE_TRANSFER_READ, 0, 0,
	                                    this_surface->width(),
	                                    this_surface->height());

	if (!transfer) {
		Genode::error("could not create transfer object");
		return FALSE;
	}

	void *data = screen->transfer_map(screen, transfer);
	if (!data) {
		Genode::error("transfer failed");
		screen->tex_transfer_destroy(transfer);
		return FALSE;
	}

#ifdef __i386__
	/* flush cache */
	if (do_clflush) {
		volatile char *virt_addr = (volatile char *)data;
		long num_bytes = transfer->stride*transfer->height;
		asm volatile ("": : :"memory");
		enum { CACHE_LINE_SIZE = 16 /* hard-coded for now */ };
		for (long i = 0; i < num_bytes; i += CACHE_LINE_SIZE, virt_addr += CACHE_LINE_SIZE)
			asm volatile("clflush %0" : "+m" (*((volatile char *)virt_addr)));
		asm volatile ("": : :"memory");
	}
#endif

	if (this_surface->_type == TYPE_SCANOUT)
		blit(data, transfer->stride, genode_framebuffer()->local_addr(),
		     transfer->stride, transfer->stride, transfer->height);
	else if (this_surface->_type == TYPE_WINDOW)
		blit(data, transfer->stride, this_surface->_addr,
		     transfer->stride, transfer->stride, transfer->height);

	screen->transfer_unmap(screen, transfer);
	screen->tex_transfer_destroy(transfer);

	this_surface->_sequence_number++;

	if (this_surface->_type == TYPE_SCANOUT)
		genode_framebuffer()->flush();

	return TRUE;
}


extern "C" const char *native_get_name(void)
{
	/*
	 * Among the 'native_' functions, this one is called first - a good
	 * opportunity to define the 'eglLog' debug level. For maximum verbosity,
	 * use '_EGL_DEBUG'.
	 */
	_eglSetLogLevel(_EGL_DEBUG);
	return "Genode-EGL";
}


extern "C" struct native_probe *
native_create_probe(EGLNativeDisplayType dpy)
{
	Genode::warning(__func__, " not yet implemented dpy=", dpy);
	return 0;
}


extern "C" enum native_probe_result
native_get_probe_result(struct native_probe *nprobe)
{
	Genode::warning(__func__, " not yet implemented");
	return NATIVE_PROBE_UNKNOWN;
}


extern "C" struct native_display *
native_create_display(EGLNativeDisplayType dpy,
                      struct native_event_handler *event_handler)
{
	/*
	 * Request API by dynamically loading the driver module. Each driver
	 * module has an entry-point function called 'drm_api_create'. For the
	 * i915 driver, this function resides in
	 * 'gallium/winsys/drm/intel/gem/intel_drm_api.c'.
	 */
	drm_api *api = 0;

	const char *driver_filename = probe_gpu_and_select_driver();
	void *driver_so_handle = driver_filename ? dlopen(driver_filename, 0) : 0;
	if (driver_so_handle) {

		/* query entry point into driver module */
		drm_api *(*drm_api_create) (void) = 0;
		drm_api_create = (drm_api *(*)(void))dlsym(driver_so_handle, "drm_api_create");
		if (drm_api_create)
			api = drm_api_create();
		else
			Genode::warning("could not obtain symbol \"drm_api_create\" in driver ",
			                "'", driver_filename, "'");
	}

	if (!api) {
		Genode::warning("falling back to softpipe driver");

		/*
		 * Performing clflush is not needed when using software rendering.
		 * Furthermore, on qemu with the default cpu, 'cflush' is an illegal
		 * instruction.
		 */
		do_clflush = false;
	}

	return new (env()->heap()) Display(api);
}
