/*
 * \brief  Redirect VirtualBox LOG output to Genode LOG
 * \author Norman Feske
 * \date   2013-08-23
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <fcntl.h>
#include <string.h>


namespace {

	struct Plugin_context : Libc::Plugin_context { };

	class Plugin : public Libc::Plugin
	{
		private:

			Plugin_context _context;

			Libc::File_descriptor *_fd;

			char const *log_file_name() const { return "/log"; }

			bool _match(char const *name) const
			{
				return strcmp(name, log_file_name()) == 0;
			}

		public:

			Plugin() :
				_fd(Libc::file_descriptor_allocator()->alloc(this, &_context))
			{ }

			bool supports_stat(const char *path)
			{
				return _match(path);
			}

			bool supports_open(const char *path, int flags)
			{
				return _match(path);
			}

			int stat(const char *path, struct stat *buf)
			{
				bool const match = _match(path);
				if (buf && match) {
					Genode::memset(buf, 0, sizeof(struct stat));
					buf->st_mode = S_IFCHR;
				}

				errno = match ? 0 : ENOENT;
				return match ? 0 : -1;
			}

			Libc::File_descriptor *open(const char *pathname, int flags)
			{
				return _match(pathname) ? _fd : 0;
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
					case F_GETFL: return O_WRONLY;
					default: Genode::error("fcntl(): command ", cmd, " not supported");
					         return -1;
				}
			}

			int fstat(Libc::File_descriptor *, struct stat *buf)
			{
				/*
				 * The following values were obtained with small test program that
				 * calls fstat for stdout on linux.
				 */
				buf->st_dev     =    11;
				buf->st_ino     =     4;
				buf->st_mode    =  8592;
				buf->st_nlink   =     1;
				buf->st_uid     =     0;
				buf->st_gid     =     0;
				buf->st_rdev    = 34818;
				buf->st_size    =     0;
				buf->st_blksize =  1024;
				buf->st_blocks  =     0;

				return 0;
			}

			ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
			{
				if (fd != _fd) {
					errno = EBADF;
					return -1;
				}

				char *src = (char *)buf;

				/* count does not include the trailing '\0' */
				int orig_count = count;
				while (count > 0) {
					char tmp[128];
					int curr_count= count > sizeof(tmp) - 1 ? sizeof(tmp) - 1 : count;
					strncpy(tmp, src, curr_count);
					tmp[curr_count > 0 ? curr_count : 0] = 0;
					Genode::log(Genode::Cstring(tmp));
					count -= curr_count;
					src   += curr_count;
				}
				return orig_count;
			}

			int ioctl(Libc::File_descriptor *, int request, char *)
			{
				/*
				 * Some programs or libraries use to perform 'TIOCGETA'
				 * operations on stdout, in particular the termios module of
				 * Python. Those programs may break if 'tcgetattr' return with
				 * an error. We pretend to be more successful than we really
				 * are to make them happy.
				 */
				return 0;
			}
	};

} /* unnamed namespace */


extern "C" void init_libc_vbox_logger(void)
{
	static Plugin plugin;
}
