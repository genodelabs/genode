/*
 * \brief  Libc plugin that uses Genode's Terminal session
 * \author Norman Feske
 * \date   2011-09-09
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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
#include <termios.h>

/* Genode includes */
#include <terminal_session/connection.h>
#include <util/misc_math.h>
#include <base/log.h>
#include <base/env.h>
#include <base/thread.h>


extern void (*libc_select_notify)();


namespace {

	typedef Genode::Thread_deprecated<4096> Read_sigh_thread;


	/**
	 * Thread for receiving notifications about data available for reading
	 * from terminal session
	 */
	class Read_sigh : Read_sigh_thread
	{
		private:

			Genode::Lock _startup_lock;

			Genode::Signal_context            _sig_ctx;
			Genode::Signal_receiver           _sig_rec;
			Genode::Signal_context_capability _sig_cap;

			void entry()
			{
				_sig_cap = _sig_rec.manage(&_sig_ctx);

				_startup_lock.unlock();

				for (;;) {
					_sig_rec.wait_for_signal();

					if (libc_select_notify)
						libc_select_notify();
				}
			}

		public:

			Read_sigh()
			:
				Read_sigh_thread("read_sigh"),
				_startup_lock(Genode::Lock::LOCKED)
			{
				start();

				/* wait until '_sig_cap' is initialized */
				_startup_lock.lock();
			}

			Genode::Signal_context_capability cap() { return _sig_cap; }
	};


	/**
	 * Return singleton instance of 'Read_sigh'
	 */
	static Genode::Signal_context_capability read_sigh()
	{
		static Read_sigh inst;
		return inst.cap();
	}


	/**
	 * An open file descriptor for this plugin corresponds to a terminal
	 * connection
	 *
	 * The terminal connection is created along with the context. The
	 * notifications about data available for reading are delivered to
	 * the 'Read_sigh' thread, which cares about unblocking 'select()'.
	 */
	class Plugin_context : public Libc::Plugin_context, public Terminal::Connection
	{
		private:

			int _status_flags;

		public:

			Plugin_context()
			: _status_flags(0)
			{
				read_avail_sigh(read_sigh());
			}

			/**
			 * Set/get file status status flags
			 */
			void status_flags(int flags) { _status_flags = flags; }
			int status_flags() { return _status_flags; }
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
			static char const *_dev_name() { return "/dev/terminal"; }

			/*
			 * Prioritize plugin over libc_vfs
			 */
			enum { PLUGIN_PRIORITY = 1 };

		public:

			/**
			 * Constructor
			 */
			Plugin() : Libc::Plugin(PLUGIN_PRIORITY) { }

			bool supports_stat(const char *path)
			{
				return (Genode::strcmp(path, "/dev") == 0) ||
				       (Genode::strcmp(path, _dev_name()) == 0);
			}

			bool supports_open(const char *path, int flags)
			{
				return (Genode::strcmp(path, _dev_name()) == 0);
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
				/*
				 * We pretent to be a character device
				 *
				 * This is important, i.e., to convince the gdbserver code to
				 * cooperate with us.
				 */
				if (buf) {
					Genode::memset(buf, 0, sizeof(struct stat));
					if (Genode::strcmp(path, "/dev") == 0)
						buf->st_mode = S_IFDIR;
					else if (Genode::strcmp(path, _dev_name()) == 0)
						buf->st_mode = S_IFCHR;
					else {
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
					buf->st_mode = S_IFCHR;
				}
				return 0;
			}


			bool supports_select(int nfds,
			                     fd_set *readfds,
			                     fd_set *writefds,
			                     fd_set *exceptfds,
			                     struct timeval *timeout)
			{
				return true;
			}

			int select(int nfds,
			           fd_set *readfds,
			           fd_set *writefds,
			           fd_set *exceptfds,
			           struct timeval *timeout)
			{
				fd_set in_readfds;
				fd_set in_writefds;
				FD_ZERO(&in_readfds);
				FD_ZERO(&in_writefds);

				if (readfds) {
					in_readfds = *readfds;
					FD_ZERO(readfds);
				}

				if (writefds) {
					in_writefds = *writefds;
					FD_ZERO(writefds);
				}

				if (exceptfds)
					FD_ZERO(exceptfds);

				int nready = 0;
				for (int libc_fd = 0; libc_fd < nfds; libc_fd++) {
					Libc::File_descriptor *fdo =
						Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

					/* handle only libc_fds that belong to this plugin */
					if (!fdo || (fdo->plugin != this))
						continue;

					if (FD_ISSET(libc_fd, &in_readfds) && context(fdo)->avail()) {
						if (readfds)
							FD_SET(libc_fd, readfds);
						nready++;
					}

					if (FD_ISSET(libc_fd, &in_writefds)) {
						if (writefds)
							FD_SET(libc_fd, writefds);
						nready++;
					}
				}
				return nready;
			}

			ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
			{
				Genode::size_t chunk_size = context(fd)->io_buffer_size();

				Genode::size_t written_bytes = 0;
				while (written_bytes < count) {

					Genode::size_t n = Genode::min(count - written_bytes, chunk_size);
					context(fd)->write((char *)buf + written_bytes, n);
					written_bytes += n;
				}

				return count;
			}

			ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
			{
				for (;;) {
					Genode::size_t num_bytes = context(fd)->read(buf, count);

					if (num_bytes)
						return num_bytes;

					/* read returned 0, block until data becomes available */
					fd_set rfds;
					FD_ZERO(&rfds);
					FD_SET(fd->libc_fd, &rfds);
					::select(fd->libc_fd + 1, &rfds, 0, 0, 0);
				}
			}

			int fcntl(Libc::File_descriptor *fd, int cmd, long arg)
			{
				switch (cmd) {
				case F_GETFL: return context(fd)->status_flags();
				case F_SETFD:
					{
						const long supported_flags = FD_CLOEXEC;
						/* if unsupported flags are used, fall through with error */
						if (!(arg & ~supported_flags)) {
							/* close fd if exec is called - no exec support -> ignore */
							if (arg & FD_CLOEXEC)
								return 0;
						}
					}
				default:
					Genode::warning("fcntl(): command ", cmd, " args ", arg, " "
					                "not supported - terminal");
					return -1;
				}
			}

			int ioctl(Libc::File_descriptor *fd, int request, char *argp)
			{
				struct termios *t = (struct termios*)argp;
				switch (request) {
				case TIOCGETA:
					Genode::memset(t,0,sizeof(struct termios));
					t->c_lflag = ECHO;
					return 0;
				case TIOCSETAW:
					return 0;
				case TIOCSETAF:
					return 0;
				case TIOCGWINSZ:
					{
						::winsize *winsize = (::winsize *)argp;
						Terminal::Session::Size terminal_size = context(fd)->size();
						winsize->ws_row = terminal_size.lines();
						winsize->ws_col = terminal_size.columns();
						return 0;
					}
				}
				return -1;
			}

			int dup2(Libc::File_descriptor *fd, Libc::File_descriptor *new_fd)
			{
				new_fd->context = fd->context;
				return new_fd->libc_fd;
			}
	};

} /* unnamed namespace */


void __attribute__((constructor)) init_libc_terminal(void)
{
	static Plugin plugin;
}
