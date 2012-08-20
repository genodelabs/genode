/*
 * \brief  Directory file system
 * \author Norman Feske
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__DIR_FILE_SYSTEM_H_
#define _NOUX__DIR_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <tar_file_system.h>
#include <fs_file_system.h>
#include <terminal_file_system.h>
#include <null_file_system.h>
#include <zero_file_system.h>

namespace Noux {

	class Dir_file_system : public File_system
	{
		public:

			enum { MAX_NAME_LEN = 128 };

		private:

			Lock _lock;

			/* pointer to first child file system */
			File_system *_first_file_system;

			/* add new file system to the list of children */
			void _append_file_system(File_system *fs)
			{
				if (!_first_file_system) {
					_first_file_system = fs;
					return;
				}

				File_system *curr = _first_file_system;
				while (curr->next)
					curr = curr->next;

				curr->next = fs;
			}

			/**
			 * Directory name
			 */
			char _name[MAX_NAME_LEN];

			bool _is_root() const { return _name[0] == 0; }

		public:

			Dir_file_system(Xml_node node) : _first_file_system(0)
			{
				/* remember directory name */
				if (node.has_type("fstab"))
					_name[0] = 0;
				else
					node.attribute("name").value(_name, sizeof(_name));

				for (unsigned i = 0; i < node.num_sub_nodes(); i++) {

					Xml_node sub_node = node.sub_node(i);

					if (sub_node.has_type("tar")) {
						_append_file_system(new Tar_file_system(sub_node));
						continue;
					}

					if (sub_node.has_type("fs")) {
						_append_file_system(new Fs_file_system(sub_node));
						continue;
					}

					/* traverse into <dir> nodes */
					if (sub_node.has_type("dir")) {
						_append_file_system(new Dir_file_system(sub_node));
						continue;
					}

					if (sub_node.has_type("terminal")) {
						_append_file_system(new Terminal_file_system(sub_node));
						continue;
					}

					if (sub_node.has_type("null")) {
						_append_file_system(new Null_file_system());

						continue;
					}

					if (sub_node.has_type("zero")) {
						_append_file_system(new Zero_file_system());

						continue;
					}

					{
						char type_name[64];
						sub_node.type_name(type_name, sizeof(type_name));
						PWRN("unknown fstab node type <%s>", type_name);
					}
				}
			}

			/**
			 * Return portion of the path after the element corresponding to
			 * the current directory.
			 */
			char const *_sub_path(char const *path) const
			{
				/* do not strip anything from the path when we are root */
				if (_is_root())
					return path;

				/* skip heading slash in path if present */
				if (path[0] == '/')
					path++;

				size_t const name_len = strlen(_name);
				if (strcmp(path, _name, name_len) != 0)
					return 0;
				path += name_len;

				/*
				 * The first characters of the first path element are equal to
				 * the current directory name. Let's check if the length of the
				 * first path element matches the name length.
				 */
				if (*path != 0 && *path != '/')
					return 0;

				return path;
			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				path = _sub_path(path);
				if (!path)
					return Dataspace_capability();

				/*
				 * Query sub file systems for dataspace using the path local to
				 * the respective file system
				 */
				File_system *fs = _first_file_system;
				for (; fs; fs = fs->next) {
					Dataspace_capability ds = fs->dataspace(path);
					if (ds.valid())
						return ds;
				}

				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
				path = _sub_path(path);
				if (!path)
					return;

				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					fs->release(path, ds_cap);
			}

			bool stat(Sysio *sysio, char const *path)
			{
				path = _sub_path(path);

				/* path does not match directory name */
				if (!path) {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				/*
				 * If path equals directory name, return information about the
				 * current directory.
				 */
				if (strlen(path) == 0) {
					sysio->stat_out.st.size = 0;
					sysio->stat_out.st.mode = Sysio::STAT_MODE_DIRECTORY | 0755;
					sysio->stat_out.st.uid  = 0;
					sysio->stat_out.st.gid  = 0;
					return true;
				}

				/*
				 * The given path refers to one of our sub directories.
				 * Propagate the request into our file systems.
				 */
				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					if (fs->stat(sysio, path))
						return true;

				/* none of our file systems felt responsible for the path */
				sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
				return false;
			}

			/**
			 * The 'path' is relative to the child file systems.
			 */
			bool _dirent_of_file_systems(Sysio *sysio, char const *path, off_t index)
			{
				int base = 0;
				for (File_system *fs = _first_file_system; fs; fs = fs->next) {

					/*
					 * Determine number of matching directory entries within
					 * the current file system.
					 */
					int const fs_num_dirent = fs->num_dirent(path);

					/*
					 * Query directory entry if index lies with the file
					 * system.
					 */
					if (index - base < fs_num_dirent) {
						index = index - base;
						bool const res = fs->dirent(sysio, path, index);
						sysio->dirent_out.entry.fileno += base;
						return res;
					}

					/* adjust base index for next file system */
					base += fs_num_dirent;
				}

				sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
				return true;
			}

			bool _dirent_of_this_dir_node(Sysio *sysio, off_t index)
			{
				if (index == 0) {
					strncpy(sysio->dirent_out.entry.name, _name,
					        sizeof(sysio->dirent_out.entry.name));

					sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY;
					sysio->dirent_out.entry.fileno = 1;
				} else
					sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				Lock::Guard guard(_lock);

				if (_is_root()) {
					return _dirent_of_file_systems(sysio, path, index);
				} else {

					if (strcmp(path, "/") == 0)
						return _dirent_of_this_dir_node(sysio, index);

					/* path contains at least one element */

					/* remove current element from path */
					path = _sub_path(path);
					if (path) {
						return _dirent_of_file_systems(sysio, path, index);

					} else {
						/* path does not lie within our tree */
						return false;
					}
				}
			}

			/*
			 * Accumulate number of directory entries that match in any of
			 * our sub file systems.
			 */
			size_t _sum_dirents_of_file_systems(char const *path)
			{
				size_t cnt = 0;
				for (File_system *fs = _first_file_system; fs; fs = fs->next) {
					cnt += fs->num_dirent(path);
				}
				return cnt;
			}

			size_t num_dirent(char const *path)
			{
				Lock::Guard guard(_lock);

				if (_is_root()) {
					return _sum_dirents_of_file_systems(path);

				} else {

					if (strcmp(path, "/") == 0)
						return 1;

					/*
					 * The path contains at least one element. Remove current
					 * element from path.
					 */
					path = _sub_path(path);

					/*
					 * If the resulting 'path' is non-NULL, the path lies
					 * within our tree. In this case, determine the sum of
					 * matching dirents of all our file systems. Otherwise,
					 * the specified path lies outside our directory node.
					 */
					return path ? _sum_dirents_of_file_systems(path) : 0;
				}
			}

			bool is_directory(char const *path)
			{
				Lock::Guard guard(_lock);

				path = _sub_path(path);
				if (!path)
					return false;

				if (strlen(path) == 0)
					return true;

				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					if (fs->is_directory(path))
						return true;

				return false;
			}

			char const *leaf_path(char const *path)
			{
				Lock::Guard guard(_lock);

				path = _sub_path(path);
				if (!path)
					return 0;

				if (strlen(path) == 0)
					return path;

				for (File_system *fs = _first_file_system; fs; fs = fs->next) {
					char const *leaf_path = fs->leaf_path(path);
					if (leaf_path)
						return leaf_path;
				}

				return 0;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				/*
				 * If 'path' is a directory, we create a 'Vfs_handle'
				 * for the root directory so that subsequent 'dirent' calls
				 * are subjected to the stacked file-system layout.
				 */
				if (is_directory(path))
					return new (env()->heap()) Vfs_handle(this, this, 0);

				/*
				 * If 'path' refers to a non-directory node, create a
				 * 'Vfs_handle' local to the file system that provides the
				 * file.
				 */
				Lock::Guard guard(_lock);

				path = _sub_path(path);

				/* check if path does not match directory name */
				if (!path)
					return 0;

				/* path equals directory name */
				if (strlen(path) == 0)
					return new (env()->heap()) Vfs_handle(this, this, 0);

				/* path refers to any of our sub file systems */
				for (File_system *fs = _first_file_system; fs; fs = fs->next) {
					Vfs_handle *vfs_handle = fs->open(sysio, path);
					if (vfs_handle)
						return vfs_handle;
				}

				/* path does not match any existing file or directory */
				return 0;
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				path = _sub_path(path);

				/* path does not match directory name */
				if (!path) {
					sysio->error.unlink = Sysio::UNLINK_ERR_NO_ENTRY;
					return false;
				}

				/*
				 * Prevent unlinking if path equals directory name defined
				 * via the static fstab configuration.
				 */
				if (strlen(path) == 0) {
					sysio->error.unlink = Sysio::UNLINK_ERR_NO_PERM;
					return false;
				}

				/*
				 * The given path refers to at least one of our sub
				 * directories. Propagate the request into all of our file
				 * systems. If at least one unlink operation succeeded, we
				 * return success.
				 */
				bool unlink_ret = false;
				Sysio::Unlink_error error = Sysio::UNLINK_ERR_NO_ENTRY;
				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					if (fs->unlink(sysio, path)) {
						unlink_ret = true;
					} else {
						/*
						 * Keep the most meaningful error code. When using
						 * stacked file systems, most child file systems will
						 * eventually return 'UNLINK_ERR_NO_ENTRY'. If any of
						 * those file systems has anything more interesting to
						 * tell (in particular 'UNLINK_ERR_NO_PERM'), return
						 * this information.
						 */
						if (sysio->error.unlink != Sysio::UNLINK_ERR_NO_ENTRY)
							error = sysio->error.unlink;
					}

				sysio->error.unlink = error;
				return unlink_ret;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
			{
				from_path = _sub_path(from_path);

				/* path does not match directory name */
				if (!from_path) {
					sysio->error.rename = Sysio::RENAME_ERR_NO_ENTRY;
					return false;
				}

				/*
				 * Prevent renaming if path equals directory name defined
				 * via the static fstab configuration.
				 */
				if (strlen(from_path) == 0) {
					sysio->error.rename = Sysio::RENAME_ERR_NO_PERM;
					return false;
				}

				/*
				 * Check if destination path resides within the same file
				 * system instance as the source path.
				 */
				to_path = _sub_path(to_path);
				if (!to_path) {
					sysio->error.rename = Sysio::RENAME_ERR_CROSS_FS;
					return false;
				}

				/* path refers to any of our sub file systems */
				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					if (fs->rename(sysio, from_path, to_path))
						return true;

				/* none of our file systems could successfully rename the path */
				return false;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				path = _sub_path(path);

				/* path does not match directory name */
				if (!path) {
					sysio->error.mkdir = Sysio::MKDIR_ERR_NO_ENTRY;
					return false;
				}

				/*
				 * Prevent mkdir of path that equals directory name defined
				 * via the static fstab configuration.
				 */
				if (strlen(path) == 0) {
					sysio->error.mkdir = Sysio::MKDIR_ERR_EXISTS;
					return false;
				}

				/* path refers to any of our sub file systems */
				for (File_system *fs = _first_file_system; fs; fs = fs->next)
					if (fs->mkdir(sysio, path))
						return true;

				/* none of our file systems could create the directory */
				return false;
			}

			/***************************
			 ** File_system interface **
			 ***************************/

			char const *name() const { return "dir"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle) { return false; }
			bool read(Sysio *sysio, Vfs_handle *vfs_handle) { return false; }
			bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) { return false; }
	};
}

#endif /* _NOUX__DIR_FILE_SYSTEM_H_ */
