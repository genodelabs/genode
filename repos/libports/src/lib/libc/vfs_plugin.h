/*
 * \brief   Libc plugin for using a process-local virtual file system
 * \author  Norman Feske
 * \date    2014-04-09
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_VFS__PLUGIN_H_
#define _LIBC_VFS__PLUGIN_H_

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <vfs/dir_file_system.h>
#include <os/config.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

namespace Libc {

	Genode::Xml_node config();
	Genode::Xml_node vfs_config();

	char const *initial_cwd();
	char const *config_stdin();
	char const *config_stdout();
	char const *config_stderr();

}


namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Libc::Plugin
{
	private:

		Genode::Allocator &_alloc;

		Vfs::Dir_file_system _root_dir;

		Genode::Xml_node _vfs_config()
		{
			try {
				return vfs_config();
			} catch (...) {
				PINF("no VFS configured");
				return Genode::Xml_node("<vfs/>");
			}
		}

		void _open_stdio(int libc_fd, char const *path, unsigned flags)
		{
			struct stat out_stat;
			if (::strlen(path) == 0 || stat(path, &out_stat) != 0)
				return;

			Libc::File_descriptor *fd = open(path, flags, libc_fd);
			if (fd->libc_fd != libc_fd) {
				PERR("could not allocate fd %d for %s, got fd %d",
				     libc_fd, path, fd->libc_fd);
				close(fd);
				return;
			}

			/*
			 * We need to manually register the path. Normally this is done
			 * by '_open'. But we call the local 'open' function directly
			 * because we want to explicitly specify the libc fd ID.
			 *
			 * We have to allocate the path from the libc (done via 'strdup')
			 * such that the path can be freed when an stdio fd is closed.
			 */
			fd->fd_path = strdup(path);
		}

	public:

		/**
		 * Constructor
		 */
		Vfs_plugin(Genode::Env &env, Genode::Allocator &alloc)
		:
			_alloc(alloc),
			_root_dir(env, _alloc, _vfs_config(),
			          Vfs::global_file_system_factory())
		{
			if (_root_dir.num_dirent("/")) {
				chdir(initial_cwd());

				_open_stdio(0, config_stdin(),  O_RDONLY);
				_open_stdio(1, config_stdout(), O_WRONLY);
				_open_stdio(2, config_stderr(), O_WRONLY);
			}
		}

		~Vfs_plugin() { }

		bool supports_access(const char *, int)                override { return true; }
		bool supports_mkdir(const char *, mode_t)              override { return true; }
		bool supports_open(const char *, int)                  override { return true; }
		bool supports_readlink(const char *, char *, ::size_t) override { return true; }
		bool supports_rename(const char *, const char *)       override { return true; }
		bool supports_rmdir(const char *)                      override { return true; }
		bool supports_stat(const char *)                       override { return true; }
		bool supports_symlink(const char *, const char *)      override { return true; }
		bool supports_unlink(const char *)                     override { return true; }
		bool supports_mmap()                                   override { return true; }

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
};

#endif
