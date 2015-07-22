/*
 * \brief  Embedded RAM VFS
 * \author Emery Hemingway
 * \date   2015-07-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__RAM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__RAM_FILE_SYSTEM_H_

#include <vfs/dir_file_system.h>
#include <ram_fs/directory.h>

namespace Vfs {
	using namespace ::File_system;
	class Ram_file_system;
}


class Vfs::Ram_file_system : public File_system, private ::File_system::Directory
{
	private:

		struct Ram_vfs_handle : Vfs_handle
		{
			private:

				::File_system::Node *_node;

			public:

				Ram_vfs_handle(File_system &fs, int status_flags,
				               ::File_system::Node *node)
				: Vfs_handle(fs, fs, status_flags), _node(node)
				{ }

				::File_system::Node *node() const { return _node; }
		};

	public:

		/**
		 * Constructor
		 */
		Ram_file_system(Xml_node config)
		: Directory("/") { }

		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name() { return "ram"; }

		void sync() { }

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count)
		{
			Ram_vfs_handle const *handle = static_cast<Ram_vfs_handle *>(vfs_handle);

			out_count = handle->node()->write(buf, buf_size, handle->seek());
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count)
		{
			Ram_vfs_handle const *handle = static_cast<Ram_vfs_handle *>(vfs_handle);

			out_count = handle->node()->read(dst, count, handle->seek());
			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len)
		{
			Ram_vfs_handle const *handle = static_cast<Ram_vfs_handle *>(vfs_handle);

			File *file = dynamic_cast<File *>(handle->node());
			if (!file)
				return FTRUNCATE_ERR_INTERRUPT;

			file->truncate(len);
			return FTRUNCATE_OK;
		}

		void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                              Signal_context_capability sigh)
		{ }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path)
		{
			Ram_dataspace_capability ds_cap;
			char *local_addr = 0;
			try {
				File *file = lookup_and_lock_file(path);
				Node_lock_guard guard(file);

				ds_cap = env()->ram_session()->alloc(file->length());

				local_addr = env()->rm_session()->attach(ds_cap);
				file->read(local_addr, file->length(), 0);
				env()->rm_session()->detach(local_addr);

			} catch(...) {
				env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
				return Dataspace_capability();
			}
			return ds_cap;
		}

		void release(char const *path, Dataspace_capability ds_cap)
		{
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		Open_result open(char const *path, unsigned mode, Vfs_handle **handle)
		{
			bool const create = mode & OPEN_MODE_CREATE;
			char const *name = basename(++path);
			Node *node;

			try {
				if (create) {
					Directory *parent = lookup_and_lock_parent(path);
					Node_lock_guard parent_guard(parent);

					if (parent->has_sub_node_unsynchronized(name))
						return OPEN_ERR_EXISTS;

					node = new (env()->heap())
						File(*env()->heap(), name);

					parent->adopt_unsynchronized(node);

				} else {
					node = lookup_and_lock(path);
					node->unlock();
				}
			} catch (Lookup_failed) {
				return OPEN_ERR_UNACCESSIBLE;
			}

			*handle = new (env()->heap())
				Ram_vfs_handle(*this, mode, node);

			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &stat)
		{
			try {
				Node *node = lookup_and_lock(++path);
				Node_lock_guard node_guard(node);

				memset(&stat, 0x00, sizeof(stat));
				stat.inode = node->inode();

				File *file = dynamic_cast<File *>(node);
				if (file) {
					stat.size = file->length();
					stat.mode = STAT_MODE_FILE | 0777;
					return STAT_OK;
				}

				Directory *dir = dynamic_cast<Directory *>(node);
				if (dir) {
					stat.size = dir->num_entries();
					stat.mode = STAT_MODE_DIRECTORY | 0777;
					return STAT_OK;
				}

				Symlink *symlink = dynamic_cast<Symlink *>(node);
				if (symlink) {
					stat.size = symlink->length();
					stat.mode = STAT_MODE_SYMLINK | 0777;
					return STAT_OK;
				}

			} catch (Lookup_failed) { }
			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &dirent)
		{
			try {
				Directory *dir = lookup_and_lock_dir(++path);
				Node_lock_guard guard(dir);

				Node *node = dir->entry_unsynchronized(index);
				if (!node) {
					dirent.fileno = 0;
					dirent.type = DIRENT_TYPE_END;
					dirent.name[0] = '\0';
					return DIRENT_OK;
				}

				dirent.fileno = index+1;

				if (dynamic_cast<File      *>(node))
					dirent.type = DIRENT_TYPE_FILE;
				else if (dynamic_cast<Directory *>(node))
					dirent.type = DIRENT_TYPE_DIRECTORY;
				else if (dynamic_cast<Symlink   *>(node))
					dirent.type = DIRENT_TYPE_SYMLINK;

				strncpy(dirent.name, node->name(), sizeof(dirent.name));

			} catch (Lookup_failed) {
				return DIRENT_ERR_INVALID_PATH;
			}
			return DIRENT_OK;
		}

		Unlink_result unlink(char const *path)
		{
			try {
				Directory *parent = lookup_and_lock_parent(++path);
				Node_lock_guard parent_guard(parent);

				char const *name = basename(path);

				Node *node = parent->lookup_and_lock(name);
				parent->discard_unsynchronized(node);
				destroy(env()->heap(), node);
			} catch (Lookup_failed) {
				return UNLINK_ERR_NO_ENTRY;
			}
			return UNLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len)
		{
			try {
				Symlink *symlink = lookup_and_lock_symlink(++path);
				Node_lock_guard guard(symlink);

				out_len = symlink->read(buf, buf_size, 0);
			} catch (Lookup_failed) {
				return READLINK_ERR_NO_ENTRY;
			}
			return READLINK_OK;
		}

		Rename_result rename(char const *from, char const *to)
		{
			try {
				Directory *from_dir = lookup_and_lock_parent(++from);
				Node_lock_guard from_guard(from_dir);
				Directory   *to_dir = lookup_and_lock_parent(++to);
				Node_lock_guard to_guard(to_dir);

				Node *node = from_dir->lookup_and_lock(basename(from));
				Node_lock_guard guard(node);

				from_dir->discard_unsynchronized(node);
				node->name(basename(to));
				to_dir->adopt_unsynchronized(node);

			} catch (Lookup_failed) {
				return RENAME_ERR_NO_ENTRY;
			}
			return RENAME_OK;
		}

		Mkdir_result mkdir(char const *path, unsigned mode)
		{
			try {
				Directory *parent = lookup_and_lock_parent(++path);
				Node_lock_guard parent_guard(parent);

				char const *name = basename(path);

				if (strlen(name) > MAX_NAME_LEN)
					return MKDIR_ERR_NAME_TOO_LONG;

				if (parent->has_sub_node_unsynchronized(name))
					return MKDIR_ERR_EXISTS;

				parent->adopt_unsynchronized(new (env()->heap())
					Directory(name));

			} catch (Lookup_failed) {
				return MKDIR_ERR_NO_ENTRY;
			} catch (Allocator::Out_of_memory) {
				return MKDIR_ERR_NO_SPACE;
			}
			return MKDIR_OK;
		}

		Symlink_result symlink(char const *from, char const *to)
		{
			try {
				Directory *parent = lookup_and_lock_parent(++to);
				Node_lock_guard parent_guard(parent);

				char const *name = basename(to);

				if (strlen(name) > MAX_NAME_LEN)
					return SYMLINK_ERR_NAME_TOO_LONG;

				if (parent->has_sub_node_unsynchronized(name))
					return SYMLINK_ERR_EXISTS;

				Symlink * const symlink = new (env()->heap())
					Symlink(name);

				parent->adopt_unsynchronized(symlink);
				symlink->write(from, strlen(from), 0);

			} catch (Lookup_failed) {
				return SYMLINK_ERR_NO_ENTRY;
			} catch (Allocator::Out_of_memory) {
				return SYMLINK_ERR_NO_SPACE;
			}
			return SYMLINK_OK;
		}

		file_size num_dirent(char const *path)
		{
			try {
				Directory *dir = lookup_and_lock_dir(++path);
				Node_lock_guard guard(dir);
				return dir->num_entries();
			} catch (Lookup_failed) {
				return 0;
			}
		}

		bool is_directory(char const *path)
		{
			try {
				lookup_and_lock_dir(++path)->unlock();
				return true;
			} catch (...) {
				return false;
			}
		}

		char const *leaf_path(char const *path)
		{
			try {
				lookup_and_lock(path+1)->unlock();
				return path;
			} catch (...) { }
			return 0;
		}

};

#endif /* _INCLUDE__VFS__RAM_FILE_SYSTEM_H_ */
