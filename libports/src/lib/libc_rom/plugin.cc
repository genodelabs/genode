/*
 * \brief  Libc plugin that uses Genode's ROM session
 * \author Norman Feske
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_rom_dataspace.h>
#include <util/misc_math.h>
#include <base/printf.h>
#include <base/env.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace {

	/**
	 * An open file descriptor for this plugin corresponds to a ROM connection
	 *
	 * The connection is created along with the context.
	 */
	struct Plugin_context : Libc::Plugin_context, Genode::Attached_rom_dataspace
	{
		Genode::off_t read_offset;

		Plugin_context(char const *filename)
		: Genode::Attached_rom_dataspace(filename), read_offset(0) { }
	};


	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}


	class Plugin : public Libc::Plugin
	{
		private:

			bool _probe_rom(char const *filename)
			{
				try {
					/*
					 * Create ROM connection as local variable. The connection
					 * gets closed automatically when leaving the scope of this
					 * function.
					 */
					Genode::Rom_connection rom(filename);
					return true;
				} catch (...) {
					return false;
				}
			}

		public:

			/**
			 * Constructor
			 */
			Plugin()
			{ }

			bool supports_open(const char *path, int flags)
			{
				return _probe_rom(&path[1]);
			}

			bool supports_stat(const char *path)
			{
				return _probe_rom(&path[1]);
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				Plugin_context *context = new (Genode::env()->heap())
				                          Plugin_context(&pathname[1]);
				return Libc::file_descriptor_allocator()->alloc(this, context);
			}

			int close(Libc::File_descriptor *fd)
			{
				Genode::destroy(Genode::env()->heap(), context(fd));
				Libc::file_descriptor_allocator()->free(fd);
				return 0;
			}

			int stat(const char *path, struct stat *buf)
			{
				Genode::Rom_connection rom(&path[1]);

				memset(buf, 0, sizeof(struct stat));
				buf->st_mode = S_IFREG;
				buf->st_size = Genode::Dataspace_client(rom.dataspace()).size();
				return 0;
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				Plugin_context *rom = context(fd);

				/* file read limit is the size of data space */
				Genode::size_t const max_size = rom->size();

				/* shortcut to current read offset */
				Genode::off_t &read_offset = rom->read_offset;

				/* maximum read offset, clamped to dataspace size */
				Genode::off_t const end_offset = Genode::min(count + read_offset, max_size);

				/* source address within the dataspace */
				char const *src = rom->local_addr<char>() + read_offset;

				/* check if end of file is reached */
				if (read_offset >= end_offset)
					return 0;

				/* copy-out bytes from ROM dataspace */
				Genode::off_t num_bytes = end_offset - read_offset;
				memcpy(buf, src, num_bytes);

				/* advance read offset */
				read_offset += num_bytes;

				return num_bytes;
			}

			::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
			{
				Plugin_context *rom = context(fd);

				switch (whence) {

				case SEEK_CUR:

					offset += rom->read_offset;

					/*
					 * falling through...
					 */

				case SEEK_SET:

					if (offset > (::off_t)rom->size()) {
						errno = EINVAL;
						return (::off_t)(-1);
					}

					rom->read_offset = offset;
					break;

				case SEEK_END:

					rom->read_offset = rom->size();
					break;

				default:
					errno = EINVAL;
					return (::off_t)(-1);
				}

				return rom->read_offset;
			}
	};

} /* unnamed namespace */


void __attribute__((constructor)) init_libc_rom(void)
{
	static Plugin plugin;
}
