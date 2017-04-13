/*
 * \brief  Libc plugin for using a process-local virtual file system
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
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

namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Libc::Plugin
{
	private:

		Genode::Allocator &_alloc;

		Vfs::File_system &_root_dir;

		void _open_stdio(Genode::Xml_node const &node, char const *attr,
		                 int libc_fd, unsigned flags)
		{
			try {
				Genode::String<Vfs::MAX_PATH_LEN> path;
				struct stat out_stat;

				node.attribute(attr).value(&path);

				if (stat(path.string(), &out_stat) != 0)
					return;

				Libc::File_descriptor *fd = open(path.string(), flags, libc_fd);
				if (fd->libc_fd != libc_fd) {
					Genode::error("could not allocate fd ",libc_fd," for ",path,", "
					              "got fd ",fd->libc_fd);
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
				if (fd->fd_path) { Genode::warning("may leak former FD path memory"); }
				fd->fd_path = strdup(path.string());

			} catch (Xml_node::Nonexistent_attribute) { }
		}

	public:

		Vfs_plugin(Libc::Env &env, Genode::Allocator &alloc)
		:
			_alloc(alloc), _root_dir(env.vfs())
		{
			using Genode::Xml_node;

			if (_root_dir.num_dirent("/"))
				env.config([&] (Xml_node const &top) {
					try {
						Xml_node const node = top.sub_node("libc");

						try {
							Genode::String<Vfs::MAX_PATH_LEN> path;
							node.attribute("cwd").value(&path);
							chdir(path.string());
						} catch (Xml_node::Nonexistent_attribute) { }

						_open_stdio(node, "stdin",  0, O_RDONLY);
						_open_stdio(node, "stdout", 1, O_WRONLY);
						_open_stdio(node, "stderr", 2, O_WRONLY);
					} catch (Xml_node::Nonexistent_sub_node) { }
				});
		}

		~Vfs_plugin() final { }

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
