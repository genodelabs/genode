/*
 * \brief  Libc plugin for using a process-local virtual file system
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_VFS__PLUGIN_H_
#define _LIBC_VFS__PLUGIN_H_

/* Genode includes */
#include <libc/component.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* local includes */
#include "task.h"
#include "libc_errno.h"


namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Libc::Plugin
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::File_system         &_root_dir;
		Vfs::Io_response_handler &_response_handler;

		/**
		 * Sync a handle and propagate errors
		 */
		int _vfs_sync(Vfs::Vfs_handle&);

	public:

		Vfs_plugin(Libc::Env                &env,
		           Genode::Allocator        &alloc,
		           Vfs::Io_response_handler &handler)
		:
			_alloc(alloc), _root_dir(env.vfs()), _response_handler(handler)
		{ }

		~Vfs_plugin() final { }

		bool root_dir_has_dirents() const { return _root_dir.num_dirent("/") > 0; }

		bool supports_access(const char *, int)                override { return true; }
		bool supports_mkdir(const char *, mode_t)              override { return true; }
		bool supports_open(const char *, int)                  override { return true; }
		bool supports_poll()                                   override { return true; }
		bool supports_readlink(const char *, char *, ::size_t) override { return true; }
		bool supports_rename(const char *, const char *)       override { return true; }
		bool supports_rmdir(const char *)                      override { return true; }
		bool supports_stat(const char *)                       override { return true; }
		bool supports_symlink(const char *, const char *)      override { return true; }
		bool supports_unlink(const char *)                     override { return true; }
		bool supports_mmap()                                   override { return true; }

		bool supports_select(int nfds,
		                     fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		                     struct timeval *timeout) override;

		Libc::File_descriptor *open(const char *, int, int libc_fd);

		Libc::File_descriptor *open(const char *path, int flags) override
		{
			return open(path, flags, Libc::ANY_FD);
		}

		int     access(char const *, int) override;
		int     close(Libc::File_descriptor *) override;
		int     dup2(Libc::File_descriptor *, Libc::File_descriptor *) override;
		int     fcntl(Libc::File_descriptor *, int, long) override;
		int     fstat(Libc::File_descriptor *, struct stat *) override;
		int     fstatfs(Libc::File_descriptor *, struct statfs *) override;
		int     fsync(Libc::File_descriptor *fd) override;
		int     ftruncate(Libc::File_descriptor *, ::off_t) override;
		ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t , ::off_t *) override;
		int     ioctl(Libc::File_descriptor *, int , char *) override;
		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence) override;
		int     mkdir(const char *, mode_t) override;
		bool    poll(File_descriptor &fdo, struct pollfd &pfd) override;
		ssize_t read(Libc::File_descriptor *, void *, ::size_t) override;
		ssize_t readlink(const char *, char *, ::size_t) override;
		int     rename(const char *, const char *) override;
		int     rmdir(const char *) override;
		int     stat(const char *, struct stat *) override;
		int     symlink(const char *, const char *) override;
		int     unlink(const char *) override;
		ssize_t write(Libc::File_descriptor *, const void *, ::size_t ) override;
		void   *mmap(void *, ::size_t, int, int, Libc::File_descriptor *, ::off_t) override;
		int     munmap(void *, ::size_t) override;
		int     select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) override;
};

#endif
