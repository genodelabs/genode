/*
 * \brief  Pipe plugin implementation
 * \author Christian Prochaska
 * \date   2014-07-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/ring_buffer.h>
#include <util/misc_math.h>

/* libc includes */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

/* libc plugin interface */
#include <libc-plugin/fd_alloc.h>
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>


/* function to notify libc about a socket event */
extern void (*libc_select_notify)();


namespace Libc_pipe {

	using namespace Genode;

	enum Type { READ_END, WRITE_END };
	enum { PIPE_BUF_SIZE = 4096 };

	typedef Ring_buffer<unsigned char, PIPE_BUF_SIZE+1> Pipe_buffer;

	class Plugin_context : public Libc::Plugin_context
	{
		private:

			Type _type;

			Libc::File_descriptor *_partner;

			Genode::Allocator &_alloc;

			Pipe_buffer *_buffer;

			Genode::Semaphore *_write_avail_sem;

			bool _nonblock = false;

		public:

			/**
			 * Constructor
			 *
			 * \param type     information if the file descriptor belongs to the
			 *                 read end or to the write end of the pipe
			 *
			 * \param partner  the other pipe end
			 */
			Plugin_context(Type type, Libc::File_descriptor *partner,
			               Genode::Allocator &alloc);

			~Plugin_context();

			Type type() const                          { return _type; }
			Pipe_buffer *buffer() const                { return _buffer; }
			Libc::File_descriptor *partner() const     { return _partner; }
			Genode::Semaphore *write_avail_sem() const { return _write_avail_sem; }
			bool nonblock() const                      { return _nonblock; }

			void set_partner(Libc::File_descriptor *partner) { _partner = partner; }
			void set_nonblock(bool nonblock) { _nonblock = nonblock; }
	};


	class Plugin : public Libc::Plugin
	{
		private:

			Genode::Constructible<Genode::Heap> _heap;

		public:

			/**
			 * Constructor
			 */
			Plugin();

			void init(Genode::Env &env) override;

			bool supports_pipe() override;
			bool supports_select(int nfds,
			                     fd_set *readfds,
			                     fd_set *writefds,
			                     fd_set *exceptfds,
			                     struct timeval *timeout) override;

			int close(Libc::File_descriptor *pipefdo) override;
			int fcntl(Libc::File_descriptor *pipefdo, int cmd, long arg) override;
			int pipe(Libc::File_descriptor *pipefdo[2]) override;
			ssize_t read(Libc::File_descriptor *pipefdo, void *buf,
			             ::size_t count) override;
			int select(int nfds, fd_set *readfds, fd_set *writefds,
			           fd_set *exceptfds, struct timeval *timeout) override;
			ssize_t write(Libc::File_descriptor *pipefdo, const void *buf,
			              ::size_t count) override;
	};


	/***************
	 ** Utilities **
	 ***************/

	Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}


	static inline bool read_end(Libc::File_descriptor *fdo)
	{
		return (context(fdo)->type() == READ_END);
	}


	static inline bool write_end(Libc::File_descriptor *fdo)
	{
		return (context(fdo)->type() == WRITE_END);
	}


	/********************
	 ** Plugin_context **
	 ********************/

	Plugin_context::Plugin_context(Type type, Libc::File_descriptor *partner,
	                               Genode::Allocator &alloc)
	: _type(type), _partner(partner), _alloc(alloc)
	{
		if (!_partner) {

			/* allocate shared resources */

			_buffer = new (_alloc) Pipe_buffer;
			_write_avail_sem = new (_alloc) Genode::Semaphore(PIPE_BUF_SIZE);

		} else {

			/* get shared resource pointers from partner */

			_buffer     = context(_partner)->buffer();
			_write_avail_sem = context(_partner)->write_avail_sem();
		}
	}


	Plugin_context::~Plugin_context()
	{
		if (_partner) {

			/* remove the fd this context belongs to from the partner's context */
			context(_partner)->set_partner(0);

		} else {

			/* partner fd is already destroyed -> free shared resources */
			destroy(_alloc, _buffer);
			destroy(_alloc, _write_avail_sem);
		}
	}


	/************
	 ** Plugin **
	 ************/

	Plugin::Plugin()
	{
		Genode::log("using the pipe libc plugin");
	}


	void Plugin::init(Genode::Env &env)
	{
		_heap.construct(env.ram(), env.rm());
	}


	bool Plugin::supports_pipe()
	{
		return true;
	}


	bool Plugin::supports_select(int nfds,
	                             fd_set *readfds,
	                             fd_set *writefds,
	                             fd_set *exceptfds,
	                             struct timeval *timeout)
	{
		/*
		 * Return true if any file descriptor which is marked set in one of
		 * the sets belongs to this plugin
		 */
		for (int libc_fd = 0; libc_fd < nfds; libc_fd++) {

			if (FD_ISSET(libc_fd, readfds) ||
			    FD_ISSET(libc_fd, writefds) ||
			    FD_ISSET(libc_fd, exceptfds)) {

				Libc::File_descriptor *fdo =
					Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

				if (fdo && (fdo->plugin == this))
					return true;
			}
		}
		return false;
	}


	int Plugin::close(Libc::File_descriptor *pipefdo)
	{
		Genode::destroy(*_heap, context(pipefdo));
		Libc::file_descriptor_allocator()->free(pipefdo);

		return 0;
	}


	int Plugin::fcntl(Libc::File_descriptor *pipefdo, int cmd, long arg)
	{
		switch (cmd) {

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

			case F_GETFL:

				if (write_end(pipefdo))
					return O_WRONLY;
				else
					return O_RDONLY;

			case F_SETFL:
				{
					/*
					 * O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_EXCL
					 * are ignored
					 */
					constexpr long supported_flags = O_NONBLOCK
						| O_RDONLY | O_WRONLY | O_RDWR
						| O_CREAT | O_TRUNC | O_EXCL;

					context(pipefdo)->set_nonblock(arg & O_NONBLOCK);

					if ((arg & ~supported_flags) == 0)
						return 0;

					/* unsupported flags present */

					Genode::error(__PRETTY_FUNCTION__, ": "
					              "command F_SETFL arg ", arg, " not fully supported");

					return -1;
				}

			default:

				Genode::error(__PRETTY_FUNCTION__, "s: command ", cmd, " "
				              "arg ", arg, " not supported");
				return -1;
		}

		return -1;
	}


	int Plugin::pipe(Libc::File_descriptor *pipefdo[2])
	{
		pipefdo[0] = Libc::file_descriptor_allocator()->alloc(this,
		               new (*_heap) Plugin_context(READ_END, 0, *_heap));
		pipefdo[1] = Libc::file_descriptor_allocator()->alloc(this,
		               new (*_heap) Plugin_context(WRITE_END, pipefdo[0], *_heap));
		static_cast<Plugin_context *>(pipefdo[0]->context)->set_partner(pipefdo[1]);

		return 0;
	}


	ssize_t Plugin::read(Libc::File_descriptor *fdo, void *buf, ::size_t count)
	{
		if (!read_end(fdo)) {
			Genode::error("cannot read from write end of pipe");
			errno = EBADF;
			return -1;
		}

		if (!context(fdo)->partner())
			return 0;

		if (context(fdo)->nonblock() && context(fdo)->buffer()->empty()) {
			errno = EAGAIN;
			return -1;
		}

		/* blocking mode, read at least one byte */

		ssize_t num_bytes_read = 0;

		do {

			((unsigned char*)buf)[num_bytes_read] =
				context(fdo)->buffer()->get();

			num_bytes_read++;

			context(fdo)->write_avail_sem()->up();

		} while ((num_bytes_read < (ssize_t)count) &&
		         !context(fdo)->buffer()->empty());

		return num_bytes_read;
	}


	/* no support for execptfds right now */

	int Plugin::select(int nfds,
	                   fd_set *readfds,
	                   fd_set *writefds,
	                   fd_set *exceptfds,
	                   struct timeval *timeout)
	{
		int nready = 0;
		Libc::File_descriptor *fdo;
		fd_set in_readfds, in_writefds;

		in_readfds = *readfds;
		FD_ZERO(readfds);
		in_writefds = *writefds;
		FD_ZERO(writefds);
		FD_ZERO(exceptfds);

		for (int libc_fd = 0; libc_fd < nfds; libc_fd++) {

			fdo = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

			/* handle only libc_fds that belong to this plugin */
			if (!fdo || (fdo->plugin != this))
				continue;

			if (FD_ISSET(libc_fd, &in_readfds) &&
				read_end(fdo) &&
				!context(fdo)->buffer()->empty()) {
				FD_SET(libc_fd, readfds);
				nready++;
			}

			if (FD_ISSET(libc_fd, &in_writefds) &&
			    write_end(fdo) &&
			    (context(fdo)->buffer()->avail_capacity() > 0)) {
				FD_SET(libc_fd, writefds);
				nready++;
			}
		}
		return nready;
	}


	ssize_t Plugin::write(Libc::File_descriptor *fdo, const void *buf,
	                      ::size_t count)
	{
		if (!write_end(fdo)) {
			Genode::error("cannot write into read end of pipe");
			errno = EBADF;
			return -1;
		}

		if (context(fdo)->nonblock() &&
		    (context(fdo)->buffer()->avail_capacity() == 0)) {
			errno = EAGAIN;
			return -1;
		}

		::size_t num_bytes_written = 0;
		while (num_bytes_written < count) {

			if (context(fdo)->buffer()->avail_capacity() == 0) {

				if (context(fdo)->nonblock())
					return num_bytes_written;

				if (libc_select_notify)
					libc_select_notify();
			}

			context(fdo)->write_avail_sem()->down();
		
			context(fdo)->buffer()->add(((unsigned char*)buf)[num_bytes_written]);
			num_bytes_written++;
		}

		if (libc_select_notify)
			libc_select_notify();

		return num_bytes_written;
	}
}


void __attribute__((constructor)) init_libc_pipe()
{
	static Libc_pipe::Plugin plugin;
}
