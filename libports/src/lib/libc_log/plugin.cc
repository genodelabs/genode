/*
 * \brief  Libc plugin that uses Genode's LOG service as stdout
 * \author Norman Feske
 * \date   2011-02-15
 *
 * The plugin allocates the file descriptors 1 and 2 and forwards write
 * operations referring to those descriptors to Genode's LOG service.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <fcntl.h>
#include <string.h>

/* interface to 'log_console' */
extern "C" int stdout_write(const char *);


namespace {


	struct Plugin_context : Libc::Plugin_context { };


	class Plugin : public Libc::Plugin
	{
		private:

			Plugin_context _context;

			Libc::File_descriptor *_stdout;
			Libc::File_descriptor *_stderr;

		public:

			/**
			 * Constructor
			 */
			Plugin() :
				_stdout(Libc::file_descriptor_allocator()->alloc(this, &_context, 1)),
				_stderr(Libc::file_descriptor_allocator()->alloc(this, &_context, 2))
			{ }

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
					case F_GETFL: return O_WRONLY;
					default: PERR("fcntl(): command %d not supported", cmd); return -1;
				}
			}

			/*
			 * We provide fstat here because printf inqueries _fstat about stdout
			 */
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
				if (fd != _stdout && fd != _stderr) {
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
					stdout_write(tmp);
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


void __attribute__((constructor)) init_libc_log(void)
{
	static Plugin plugin;
}
