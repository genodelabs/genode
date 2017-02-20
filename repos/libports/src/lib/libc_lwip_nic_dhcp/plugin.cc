/*
 * \brief  Libc plugin providing lwIP's DNS server address in the
 *         '/etc/resolv.conf' file
 * \author Christian Prochaska
 * \date   2013-05-02
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* lwip includes */
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>

/* fix redefinition warnings */
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#undef BYTE_ORDER

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

/* Genode includes */
#include <base/env.h>
#include <util/misc_math.h>


namespace {

	class Plugin_context : public Libc::Plugin_context
	{
		private:

			int _status_flags;
			off_t _seek_offset;

		public:

			Plugin_context() : _status_flags(0), _seek_offset(0) { }

			/**
			 * Set/get file status status flags
			 */
			void status_flags(int flags) { _status_flags = flags; }
			int status_flags() { return _status_flags; }

			/**
			 * Set seek offset
			 */
			void seek_offset(size_t seek_offset) { _seek_offset = seek_offset; }

			/**
			 * Return seek offset
			 */
			off_t seek_offset() const { return _seek_offset; }

			/**
			 * Advance current seek position by 'incr' number of bytes
			 */
			void advance_seek_offset(size_t incr)
			{
				_seek_offset += incr;
			}

			void infinite_seek_offset()
			{
				_seek_offset = ~0;
			}

	};


	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}


	class Plugin : public Libc::Plugin
	{
		private:

			/**
			 * File name this plugin feels responsible for
			 */
			static char const *_file_name() { return "/etc/resolv.conf"; }

			const char *_file_content()
			{
				static char result[32];
				ip_addr_t nameserver_ip = dns_getserver(0);
				snprintf(result, sizeof(result), "nameserver %s\n",
				         ipaddr_ntoa(&nameserver_ip));
				return result;
			}

			::off_t _file_size(Libc::File_descriptor *fd)
			{
				struct stat stat_buf;
				if (fstat(fd, &stat_buf) == -1)
					return -1;
				return stat_buf.st_size;
			}

		public:

			/**
			 * Constructor
			 */
			Plugin() { }

			bool supports_stat(const char *path)
			{
				return (Genode::strcmp(path, "/etc") == 0) ||
				       (Genode::strcmp(path, _file_name()) == 0);
			}

			bool supports_open(const char *path, int flags)
			{
				return (Genode::strcmp(path, _file_name()) == 0);
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				Plugin_context *context = new (Genode::env()->heap()) Plugin_context;
				context->status_flags(flags);
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
				if (buf) {
					Genode::memset(buf, 0, sizeof(struct stat));
					if (Genode::strcmp(path, "/etc") == 0)
						buf->st_mode = S_IFDIR;
					else if (Genode::strcmp(path, _file_name()) == 0) {
						buf->st_mode = S_IFREG;
						buf->st_size = strlen(_file_content()) + 1;
					} else {
						errno = ENOENT;
						return -1;
					}
				}
				return 0;
			}

			int fstat(Libc::File_descriptor *fd, struct stat *buf)
			{
				if (buf) {
					Genode::memset(buf, 0, sizeof(struct stat));
					buf->st_mode = S_IFREG;
					buf->st_size = strlen(_file_content()) + 1;
				}
				return 0;
			}

			::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
			{
				switch (whence) {

				case SEEK_SET:
					context(fd)->seek_offset(offset);
					return offset;

				case SEEK_CUR:
					context(fd)->advance_seek_offset(offset);
					return context(fd)->seek_offset();

				case SEEK_END:
					if (offset != 0) {
						errno = EINVAL;
						return -1;
					}
					context(fd)->infinite_seek_offset();
					return _file_size(fd);

				default:
					errno = EINVAL;
					return -1;
				}
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				::off_t seek_offset = context(fd)->seek_offset();

				if (seek_offset >= _file_size(fd))
					return 0;

				const char *content = _file_content();
				count = Genode::min((::off_t)count, _file_size(fd) - seek_offset);

				memcpy(buf, &content[seek_offset], count);

				context(fd)->advance_seek_offset(count);

				return count;
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
					case F_GETFL: return context(fd)->status_flags();
					default: Genode::error("fcntl(): command ", cmd, " not supported", cmd); return -1;
				}
			}
	};

} /* unnamed namespace */


void create_etc_resolv_conf_plugin()
{
	static Plugin plugin;
}
