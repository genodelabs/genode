/*
 * \brief  File system that hosts a single node
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__SINGLE_FILE_SYSTEM_H_
#define _INCLUDE__VFS__SINGLE_FILE_SYSTEM_H_

#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>

namespace Vfs { class Single_file_system; }


class Vfs::Single_file_system : public File_system
{
	public:

		enum Node_type {
			NODE_TYPE_FILE,        NODE_TYPE_SYMLINK,
			NODE_TYPE_CHAR_DEVICE, NODE_TYPE_BLOCK_DEVICE
		};

	private:

		Node_type const _node_type;

		enum { FILENAME_MAX_LEN = 64 };
		char _filename[FILENAME_MAX_LEN];

	protected:

		struct Single_vfs_handle : Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(char *dst, file_size count,
			                         file_size &out_count) = 0;

			virtual Write_result write(char const *src, file_size count,
			                           file_size &out_count) = 0;

			virtual bool read_ready() = 0;
		};

		struct Single_vfs_dir_handle : Single_vfs_handle
		{
			private:

				Node_type   _node_type;
				char const *_filename;

			public:

				Single_vfs_dir_handle(Directory_service &ds,
				                      File_io_service   &fs,
				                      Genode::Allocator &alloc,
				                      Node_type node_type,
				                      char const *filename)
				: Single_vfs_handle(ds, fs, alloc, 0),
				  _node_type(node_type),
				  _filename(filename)
				{ }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					out_count = 0;

					if (count < sizeof(Dirent))
						return READ_ERR_INVALID;

					file_size index = seek() / sizeof(Dirent);

					Dirent *out = (Dirent*)dst;

					if (index == 0) {
						out->fileno = (Genode::addr_t)this;
						switch (_node_type) {
						case NODE_TYPE_FILE:         out->type = DIRENT_TYPE_FILE;     break;
						case NODE_TYPE_SYMLINK:      out->type = DIRENT_TYPE_SYMLINK;  break;
						case NODE_TYPE_CHAR_DEVICE:  out->type = DIRENT_TYPE_CHARDEV;  break;
						case NODE_TYPE_BLOCK_DEVICE: out->type = DIRENT_TYPE_BLOCKDEV; break;
						}
						strncpy(out->name, _filename, sizeof(out->name));
					} else {
						out->type = DIRENT_TYPE_END;
					}

					out_count = sizeof(Dirent);

					return READ_OK;
				}

				Write_result write(char const *src, file_size count,
				                   file_size &out_count) override
				{
					return WRITE_ERR_INVALID;
				}

				bool read_ready() override { return true; }
		};

		bool _root(const char *path)
		{
			return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
		}

		bool _single_file(const char *path)
		{
			return (strlen(path) == (strlen(_filename) + 1)) &&
			       (strcmp(&path[1], _filename) == 0);
		}

	public:

		Single_file_system(Node_type node_type, char const *type_name, Xml_node config)
		:
			_node_type(node_type)
		{
			strncpy(_filename, type_name, sizeof(_filename));

			try { config.attribute("name").value(_filename, sizeof(_filename)); }
			catch (...) { }
		}


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			return Dataspace_capability();
		}

		void release(char const *path, Dataspace_capability ds_cap) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			out = Stat();
			out.device = (Genode::addr_t)this;

			if (_root(path)) {
				out.mode = STAT_MODE_DIRECTORY;

			} else if (_single_file(path)) {
				switch (_node_type) {
				case NODE_TYPE_FILE:         out.mode = STAT_MODE_FILE;     break;
				case NODE_TYPE_SYMLINK:      out.mode = STAT_MODE_SYMLINK;  break;
				case NODE_TYPE_CHAR_DEVICE:  out.mode = STAT_MODE_CHARDEV;  break;
				case NODE_TYPE_BLOCK_DEVICE: out.mode = STAT_MODE_BLOCKDEV; break;
				}
				out.inode = 1;
			} else {
				return STAT_ERR_NO_ENTRY;
			}
			return STAT_OK;
		}

		file_size num_dirent(char const *path) override
		{
			if (_root(path))
				return 1;
			else
				return 0;
		}

		bool directory(char const *path) override
		{
			if (_root(path))
				return true;

			return false;
		}

		char const *leaf_path(char const *path) override
		{
			return _single_file(path) ? path : 0;
		}

		Opendir_result opendir(char const *path, bool create,
		                       Vfs_handle **out_handle,
		                       Allocator &alloc) override
		{
			if (!_root(path))
				return OPENDIR_ERR_LOOKUP_FAILED;

			if (create)
				return OPENDIR_ERR_PERMISSION_DENIED;

			*out_handle =
				new (alloc) Single_vfs_dir_handle(*this, *this, alloc,
				                                  _node_type, _filename);
			return OPENDIR_OK;
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Unlink_result unlink(char const *) override
		{
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			if (_single_file(from) || _single_file(to))
				return RENAME_ERR_NO_PERM;
			return RENAME_ERR_NO_ENTRY;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Read_result complete_read(Vfs_handle *vfs_handle, char *dst,
		                          file_size count,
		                          file_size &out_count) override
		{
			Single_vfs_handle *handle =
				static_cast<Single_vfs_handle*>(vfs_handle);

			if (handle)
				return handle->read(dst, count, out_count);

			return READ_ERR_INVALID;
		}

		Write_result write(Vfs_handle *vfs_handle, char const *src, file_size count,
		                   file_size &out_count) override
		{
			Single_vfs_handle *handle =
				static_cast<Single_vfs_handle*>(vfs_handle);

			if (handle)
				return handle->write(src, count, out_count);

			return WRITE_ERR_INVALID;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			Single_vfs_handle *handle =
				static_cast<Single_vfs_handle*>(vfs_handle);

			if (handle)
				return handle->read_ready();

			return false;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			return FTRUNCATE_ERR_NO_PERM;
		}
};

#endif /* _INCLUDE__VFS__SINGLE_FILE_SYSTEM_H_ */
