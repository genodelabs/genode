/*
 * \brief  Libc plugin for using a process-local virtual file system
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2014-04-09
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__VFS_PLUGIN_H_
#define _LIBC__INTERNAL__VFS_PLUGIN_H_

/* Genode includes */
#include <libc/component.h>
#include <os/vfs.h>
#include <vfs/file_system.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>

/* libc-internal includes */
#include <internal/plugin.h>
#include <internal/fd_alloc.h>
#include <internal/errno.h>

namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin final : public Plugin
{
	public:

		enum class Update_mtime { NO, YES };

		/**
		 * Return path to pseudo files used for ioctl operations of a given FD
		 *
		 * The 'fd' argument must feature a valid 'fd.fd_path' member.
		 * This assumption can be violated by the stdout, stdin, or stderr FDs
		 * if the <libc> configuration lacks the corresponding attribute.
		 */
		static Absolute_path ioctl_dir(File_descriptor const &fd)
		{
			if (!fd.fd_path) {
				error("Libc::Vfs_plugin::ioctl_dir: fd lacks path information");
				class Fd_lacks_path : Exception { };
				throw Fd_lacks_path();
			}

			Absolute_path path(fd.fd_path);

			/*
			 * The pseudo files used for ioctl operations reside in a (hidden)
			 * directory named after the device path and prefixed with '.'.
			 */
			String<64> const ioctl_dir_name(".", path.last_element());

			path.strip_last_element();
			path.append_element(ioctl_dir_name.string());

			return path;
		}

	private:

		struct Mmap_entry : Registry<Mmap_entry>::Element
		{
			void            * const start;
			Vfs::Vfs_handle * const reference_handle;

			Mmap_entry(Registry<Mmap_entry> &registry, void *start,
			           Vfs::Vfs_handle *reference_handle)
			: Registry<Mmap_entry>::Element(registry, *this), start(start),
			  reference_handle(reference_handle) { }
		};

		Genode::Allocator                &_alloc;
		Vfs::File_system                 &_root_fs;
		Constructible<Genode::Directory>  _root_dir { };
		Vfs::Read_ready_response_handler &_response_handler;
		Update_mtime                const _update_mtime;
		Current_real_time                &_current_real_time;
		bool                        const _pipe_configured;
		Registry<Mmap_entry>              _mmap_registry;

		/*
		 * Cache the latest info file to accomodate highly frequent 'ioctl'
		 * calls as observed by the OSS plugin.
		 */
		struct Cached_ioctl_info : Noncopyable
		{
			Vfs_plugin                  &_vfs_plugin;
			Constructible<Readonly_file> _file { };
			Absolute_path                _path { };

			template <typename FN>
			void with_file(Absolute_path const &path, FN const &fn)
			{
				if (!_vfs_plugin._root_dir.constructed()) {
					warning("Vfs_plugin::_root_dir unexpectedly not constructed");
					return;
				}

				Directory &root_dir = *_vfs_plugin._root_dir;

				if (path != _path && root_dir.file_exists(path.string())) {
					_file.construct(root_dir, path);
					_path = path;
				}

				if (path == _path && _file.constructed())
					fn(*_file);
			}

			Cached_ioctl_info(Vfs_plugin &vfs_plugin) : _vfs_plugin(vfs_plugin) { }

		} _cached_ioctl_info { *this };

		/**
		 * Sync a handle
		 */
		void _vfs_sync(Vfs::Vfs_handle&);

		/**
		 * Update modification time
		 */
		void _vfs_write_mtime(Vfs::Vfs_handle&);

		struct Ioctl_result
		{
			bool handled;
			int  error;
		};

		/**
		 * Terminal related I/O controls
		 */
		Ioctl_result _ioctl_tio(File_descriptor *, unsigned long, char *);

		/**
		 * Block related I/O controls
		 */
		Ioctl_result _ioctl_dio(File_descriptor *, unsigned long, char *);

		/**
		 * Sound related I/O controls
		 */
		Ioctl_result _ioctl_sndctl(File_descriptor *, unsigned long, char *);

		/**
		 * Tap related I/O controls
		 */
		Ioctl_result _ioctl_tapctl(File_descriptor *, unsigned long, char *);

		/**
		 * Call functor 'fn' with ioctl info for the given file descriptor 'fd'
		 *
		 * The functor is called with an 'Xml_node' of the ioctl information
		 * as argument.
		 *
		 * If no ioctl info is available, 'fn' is not called.
		 */
		template <typename FN>
		void _with_info(File_descriptor &fd, FN const &fn);

		static bool _init_pipe_configured(Xml_node config)
		{
			bool result = false;
			config.with_optional_sub_node("libc", [&] (Xml_node libc_node) {
				result = libc_node.has_attribute("pipe"); });
			return result;
		}

	public:

		Vfs_plugin(Libc::Env                        &env,
		           Vfs::Env                         &vfs_env,
		           Genode::Allocator                &alloc,
		           Vfs::Read_ready_response_handler &handler,
		           Update_mtime                     update_mtime,
		           Current_real_time               &current_real_time,
		           Xml_node                         config)
		:
			_alloc(alloc),
			_root_fs(env.vfs_env().root_dir()),
			_response_handler(handler),
			_update_mtime(update_mtime),
			_current_real_time(current_real_time),
			_pipe_configured(_init_pipe_configured(config))
		{
			if (config.has_sub_node("libc"))
				_root_dir.construct(vfs_env);
		}

		~Vfs_plugin() final { }

		template <typename FN>
		void with_root_dir(FN const &fn)
		{
			if (_root_dir.constructed())
				fn(*_root_dir);
		}

		bool root_dir_has_dirents() const { return _root_fs.num_dirent("/") > 0; }

		bool supports_access(const char *, int)                override { return true; }
		bool supports_mkdir(const char *, mode_t)              override { return true; }
		bool supports_open(const char *, int)                  override { return true; }
		bool supports_pipe()                                   override { return _pipe_configured; }
		bool supports_poll()                                   override { return true; }
		bool supports_readlink(const char *, char *, ::size_t) override { return true; }
		bool supports_rename(const char *, const char *)       override { return true; }
		bool supports_rmdir(const char *)                      override { return true; }
		bool supports_stat(const char *)                       override { return true; }
		bool supports_symlink(const char *, const char *)      override { return true; }
		bool supports_unlink(const char *)                     override { return true; }
		bool supports_mmap()                                   override { return true; }

		/* kernel-specific API without monitor */
		File_descriptor *open_from_kernel(const char *, int, int libc_fd);
		int close_from_kernel(File_descriptor *);
		::off_t lseek_from_kernel(File_descriptor *fd, ::off_t offset);
		int     stat_from_kernel(const char *, struct stat *);

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
		int     ioctl(File_descriptor *, unsigned long, char *) override;
		::off_t lseek(File_descriptor *fd, ::off_t offset, int whence) override;
		int     mkdir(const char *, mode_t) override;
		File_descriptor *open(const char *path, int flags) override;
		int     pipe(File_descriptor *pipefdo[2]) override;
		int     poll(Pollfd fds[], int nfds) override;
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
};

#endif /* _LIBC__INTERNAL__VFS_PLUGIN_H_ */
