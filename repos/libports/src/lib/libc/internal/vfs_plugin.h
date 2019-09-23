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

#ifndef _LIBC__INTERNAL__VFS_PLUGIN_H_
#define _LIBC__INTERNAL__VFS_PLUGIN_H_

/* Genode includes */
#include <libc/component.h>
#include <vfs/file_system.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc-internal includes */
#include <internal/errno.h>


namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Plugin
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

		File_descriptor *open(const char *, int, int libc_fd);

		File_descriptor *open(const char *path, int flags) override
		{
			return open(path, flags, ANY_FD);
		}

		int     access(char const *, int) override;
		int     close(File_descriptor *) override;
		File_descriptor *dup(File_descriptor *) override;
		int     dup2(File_descriptor *, File_descriptor *) override;
		int     fcntl(File_descriptor *, int, long) override;
		int     fstat(File_descriptor *, struct stat *) override;
		int     fstatfs(File_descriptor *, struct statfs *) override;
		int     fsync(File_descriptor *fd) override;
		int     ftruncate(File_descriptor *, ::off_t) override;
		ssize_t getdirentries(File_descriptor *, char *, ::size_t , ::off_t *) override;
		int     ioctl(File_descriptor *, int , char *) override;
		::off_t lseek(File_descriptor *fd, ::off_t offset, int whence) override;
		int     mkdir(const char *, mode_t) override;
		bool    poll(File_descriptor &fdo, struct pollfd &pfd) override;
		ssize_t read(File_descriptor *, void *, ::size_t) override;
		ssize_t readlink(const char *, char *, ::size_t) override;
		int     rename(const char *, const char *) override;
		int     rmdir(const char *) override;
		int     stat(const char *, struct stat *) override;
		int     symlink(const char *, const char *) override;
		int     unlink(const char *) override;
		ssize_t write(File_descriptor *, const void *, ::size_t ) override;
		void   *mmap(void *, ::size_t, int, int, File_descriptor *, ::off_t) override;
		int     munmap(void *, ::size_t) override;
		int     select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) override;
};

#endif /* _LIBC__INTERNAL__VFS_PLUGIN_H_ */
