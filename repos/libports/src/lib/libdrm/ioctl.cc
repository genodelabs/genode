/*
 * \brief  Handler for ioctl operations on DRM device
 * \author Norman Feske
 * \date   2010-07-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/string.h>
#include <base/env.h>
#include <base/log.h>

/* libc includes */
#include <sys/ioccom.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libdrm includes */
extern "C" {
#define virtual _virtual
#include <drm.h>
#include <i915_drm.h>
#undef virtual
}

/* GPU driver interface */
#include <gpu/driver.h>

enum { verbose_ioctl = false };


long drm_command(long request) { return (request & 0xff) - DRM_COMMAND_BASE; }


/**
 * Return name of DRM command
 */
const char *command_name(long request)
{
	if (IOCGROUP(request) != DRM_IOCTL_BASE)
		return "<non-DRM>";

	switch (drm_command(request)) {
	case DRM_I915_INIT:                  return "DRM_I915_INIT";
	case DRM_I915_FLUSH:                 return "DRM_I915_FLUSH";
	case DRM_I915_FLIP:                  return "DRM_I915_FLIP";
	case DRM_I915_BATCHBUFFER:           return "DRM_I915_BATCHBUFFER";
	case DRM_I915_IRQ_EMIT:              return "DRM_I915_IRQ_EMIT";
	case DRM_I915_IRQ_WAIT:              return "DRM_I915_IRQ_WAIT";
	case DRM_I915_GETPARAM:              return "DRM_I915_GETPARAM";
	case DRM_I915_SETPARAM:              return "DRM_I915_SETPARAM";
	case DRM_I915_ALLOC:                 return "DRM_I915_ALLOC";
	case DRM_I915_FREE:                  return "DRM_I915_FREE";
	case DRM_I915_INIT_HEAP:             return "DRM_I915_INIT_HEAP";
	case DRM_I915_CMDBUFFER:             return "DRM_I915_CMDBUFFER";
	case DRM_I915_DESTROY_HEAP:          return "DRM_I915_DESTROY_HEAP";
	case DRM_I915_SET_VBLANK_PIPE:       return "DRM_I915_SET_VBLANK_PIPE";
	case DRM_I915_GET_VBLANK_PIPE:       return "DRM_I915_GET_VBLANK_PIPE";
	case DRM_I915_VBLANK_SWAP:           return "DRM_I915_VBLANK_SWAP";
	case DRM_I915_HWS_ADDR:              return "DRM_I915_HWS_ADDR";
	case DRM_I915_GEM_INIT:              return "DRM_I915_GEM_INIT";
	case DRM_I915_GEM_EXECBUFFER:        return "DRM_I915_GEM_EXECBUFFER";
	case DRM_I915_GEM_PIN:               return "DRM_I915_GEM_PIN";
	case DRM_I915_GEM_UNPIN:             return "DRM_I915_GEM_UNPIN";
	case DRM_I915_GEM_BUSY:              return "DRM_I915_GEM_BUSY";
	case DRM_I915_GEM_THROTTLE:          return "DRM_I915_GEM_THROTTLE";
	case DRM_I915_GEM_ENTERVT:           return "DRM_I915_GEM_ENTERVT";
	case DRM_I915_GEM_LEAVEVT:           return "DRM_I915_GEM_LEAVEVT";
	case DRM_I915_GEM_CREATE:            return "DRM_I915_GEM_CREATE";
	case DRM_I915_GEM_PREAD:             return "DRM_I915_GEM_PREAD";
	case DRM_I915_GEM_PWRITE:            return "DRM_I915_GEM_PWRITE";
	case DRM_I915_GEM_MMAP:              return "DRM_I915_GEM_MMAP";
	case DRM_I915_GEM_SET_DOMAIN:        return "DRM_I915_GEM_SET_DOMAIN";
	case DRM_I915_GEM_SW_FINISH:         return "DRM_I915_GEM_SW_FINISH";
	case DRM_I915_GEM_SET_TILING:        return "DRM_I915_GEM_SET_TILING";
	case DRM_I915_GEM_GET_TILING:        return "DRM_I915_GEM_GET_TILING";
	case DRM_I915_GEM_GET_APERTURE:      return "DRM_I915_GEM_GET_APERTURE";
	case DRM_I915_GEM_MMAP_GTT:          return "DRM_I915_GEM_MMAP_GTT";
	case DRM_I915_GET_PIPE_FROM_CRTC_ID: return "DRM_I915_GET_PIPE_FROM_CRTC_ID";
	case DRM_I915_GEM_MADVISE:           return "DRM_I915_GEM_MADVISE";
	case DRM_I915_OVERLAY_PUT_IMAGE:     return "DRM_I915_OVERLAY_PUT_IMAGE";
	case DRM_I915_OVERLAY_ATTRS:         return "DRM_I915_OVERLAY_ATTRS";
	case DRM_I915_GEM_EXECBUFFER2:       return "DRM_I915_GEM_EXECBUFFER2";
	default:                             return "<unknown>";
	}
}


static void dump_ioctl(long request)
{
	Genode::log("ioctl(request=", Genode::Hex(request), ", ",
	            (request & 0xe0000000) == IOC_OUT   ? "out"   :
	            (request & 0xe0000000) == IOC_IN    ? "in"    :
	            (request & 0xe0000000) == IOC_INOUT ? "inout" : "void", ", "
	            "len=", IOCPARM_LEN(request), ", "
	            "cmd=", command_name(request), ")");
}


namespace {

	struct Plugin_context : Libc::Plugin_context { };


	class Plugin : public Libc::Plugin
	{
		private:

			Gpu_driver         *_driver;
			Gpu_driver::Client *_client;

			/*
			 * Assign a priority of 1 to override libc_vfs.
			 */
			enum { PLUGIN_PRIORITY = 1 };

		public:

			Plugin(Gpu_driver *driver)
			:
				Libc::Plugin(PLUGIN_PRIORITY), _driver(driver), _client(0)
			{
				if (!_driver) {
					Genode::error("could not initialize GPU driver");
					return;
				}
				_client = _driver->create_client();
			}

			bool supports_open(const char *pathname, int flags)
			{
				return !Genode::strcmp(pathname, "/dev/drm");
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				Plugin_context *context = new (Genode::env()->heap()) Plugin_context();
				return Libc::file_descriptor_allocator()->alloc(this, context);
			}

			bool supports_stat(const char *path)
			{
				return (Genode::strcmp(path, "/dev") == 0 ||
				        Genode::strcmp(path, "/dev/drm") == 0);
			}

			int stat(const char *path, struct stat *buf)
			{
				if (buf)
					buf->st_mode = S_IFDIR;

				return 0;
			}

			int ioctl(Libc::File_descriptor *fd, int request, char *argp)
			{
				if (verbose_ioctl)
					dump_ioctl(request);

				if (drm_command(request) == DRM_I915_GEM_MMAP_GTT) {
					drm_i915_gem_mmap_gtt *arg = (drm_i915_gem_mmap_gtt *)(argp);
					arg->offset = (__u64)_driver->map_buffer_object(_client, arg->handle);
					return arg->offset ? 0 : -1;
				}
				return _driver->ioctl(_client, drm_command(request), argp);
			}

			bool supports_mmap() { return true; }

			/**
			 * Pseudo mmap specific for DRM device
			 *
			 * The original protocol between the Gallium driver and the kernel
			 * DRM driver is based on the interplay of the GEM_MMAP_GTT ioctl and
			 * mmap. First, GEM_MMAP_GTT is called with a buffer object as
			 * argument. The DRM driver returns a global ID called "fake offset"
			 * representing the buffer object. The fake offset actually refers
			 * to a region within the virtual 'dev->mm_private' memory map.
			 * The Gallium driver then passes this fake offset back to the DRM
			 * driver as 'offset' argument to 'mmap'. The DRM driver uses this
			 * argument to look up the GEM offset and set up a vm_area specific
			 * for the buffer object - paged by the DRM driver.
			 *
			 * We avoid this round-trip of a global ID. Instead of calling the
			 * 'GEM_MMAP_GTT' ioctl, we use a dedicated driver function called
			 * 'map_buffer_object', which simply returns the local address of
			 * the already mapped buffer object via the 'offset' return value.
			 * Hence, all 'mmap' has to do is passing back the 'offset' argument
			 * as return value.
			 */
			void *mmap(void *addr, ::size_t length, int prot, int flags,
			           Libc::File_descriptor *fd, ::off_t offset)
			{
				using namespace Genode;
				log(__func__, ": "
				    "addr=",   addr,       ", "
				    "length=", length,     ", "
				    "prot=",   Hex(prot),  ", "
				    "flags=",  Hex(flags), ", "
				    "offset=", Hex(offset));

				return (void *)offset;
			}

			int close(Libc::File_descriptor *fd)
			{
				Genode::destroy(Genode::env()->heap(),
				                static_cast<Plugin_context *>(fd->context));
				Libc::file_descriptor_allocator()->free(fd);
				return 0;
			}
	};
}

void __attribute__((constructor)) init_drm_device_plugin()
{
	static Plugin plugin(gpu_driver());
}
