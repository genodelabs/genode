/*
 * \brief  Support code for i915 driver
 * \author Norman Feske
 * \date   2010-07-13
 *
 * Upon startup, the Gallium i915 driver opens the file
 * "/sys/class/drm/card0/device/device" ('intel_drm_create_screen' ->
 * 'intel_drm_get_device_id'). This file contains the device ID of the GPU.
 * On Genode, there is no such file. Instead we determine the PCI device ID
 * differently, but pass it to the Gallium driver through the normal libc file
 * interface. This is achieved by adding a special libc plugin for handling´
 * this specific filename.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/snprintf.h>
#include <util/string.h>
#include <util/misc_math.h>
#include <base/env.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* GPU device driver interface */
#include <gpu/driver.h>

namespace {

	struct Plugin_context : Libc::Plugin_context
	{
		Genode::size_t position;

		Plugin_context() : position(0) { }
	};

	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}

	class Plugin : public Libc::Plugin
	{
		public:

			bool supports_open(const char *pathname, int flags)
			{
				return (Genode::strcmp(pathname,
				                       "/sys/class/drm/card0/device/device") == 0);
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				Plugin_context *context = new (Genode::env()->heap()) Plugin_context();
				return Libc::file_descriptor_allocator()->alloc(this, context);
			}

			int close(Libc::File_descriptor *fd)
			{
				Genode::destroy(Genode::env()->heap(), context(fd));
				Libc::file_descriptor_allocator()->free(fd);
				return 0;
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				/* query device ID from GPU driver */
				char device_id_string[32];
				Genode::snprintf(device_id_string, sizeof(device_id_string),
				                 "0x%x", gpu_driver()->device_id());

				if (context(fd)->position >= Genode::strlen(device_id_string))
					return 0;

				count = Genode::min(count,
				                    Genode::strlen(device_id_string -
				                                   context(fd)->position) + 1);
				Genode::strncpy((char *)buf,
				                device_id_string + context(fd)->position, count);
				context(fd)->position += count;
				return count;
			}

			bool supports_stat(const char *path)
			{
				return (Genode::strcmp(path, "/sys") == 0) ||
				       (Genode::strcmp(path, "/sys/class") == 0) ||
				       (Genode::strcmp(path, "/sys/class/drm") == 0) ||
				       (Genode::strcmp(path, "/sys/class/drm/card0") == 0) ||
				       (Genode::strcmp(path, "/sys/class/drm/card0/device") == 0) ||
				       (Genode::strcmp(path, "/sys/class/drm/card0/device/device") == 0);
			}

			int stat(const char *path, struct stat *buf)
			{
				if (buf)
					buf->st_mode = S_IFDIR;
				return 0;
			}
	};
}


void __attribute__((constructor)) init_query_device_id_plugin()
{
	static Plugin plugin;
}
