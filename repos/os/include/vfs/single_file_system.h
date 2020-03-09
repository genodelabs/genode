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
	private:

		Node_type const _type;
		Node_rwx  const _rwx;

		typedef String<64> Filename;

		Filename _filename { };

	protected:

		struct Single_vfs_handle : Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(char *dst, file_size count,
			                         file_size &out_count) = 0;

			virtual Write_result write(char const *src, file_size count,
			                           file_size &out_count) = 0;

			virtual Sync_result sync()
			{
				return SYNC_OK;
			}

			virtual bool read_ready() = 0;
		};

		struct Single_vfs_dir_handle : Single_vfs_handle
		{
			private:

				Node_type const _type;
				Node_rwx  const _rwx;

				Filename const &_filename;

				/*
				 * Noncopyable
				 */
				Single_vfs_dir_handle(Single_vfs_dir_handle const &);
				Single_vfs_dir_handle &operator = (Single_vfs_dir_handle const &);

			public:

				Single_vfs_dir_handle(Directory_service &ds,
				                      File_io_service   &fs,
				                      Genode::Allocator &alloc,
				                      Node_type          type,
				                      Node_rwx           rwx,
				                      Filename    const &filename)
				:
					Single_vfs_handle(ds, fs, alloc, 0),
					_type(type), _rwx(rwx), _filename(filename)
				{ }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					out_count = 0;

					if (count < sizeof(Dirent))
						return READ_ERR_INVALID;

					file_size index = seek() / sizeof(Dirent);

					Dirent &out = *(Dirent*)dst;

					auto dirent_type = [&] ()
					{
						switch (_type) {
						case Node_type::DIRECTORY:          return Dirent_type::DIRECTORY;
						case Node_type::SYMLINK:            return Dirent_type::SYMLINK;
						case Node_type::CONTINUOUS_FILE:    return Dirent_type::CONTINUOUS_FILE;
						case Node_type::TRANSACTIONAL_FILE: return Dirent_type::TRANSACTIONAL_FILE;
						}
						return Dirent_type::END;
					};

					if (index == 0) {
						out = {
							.fileno = (Genode::addr_t)this,
							.type   = dirent_type(),
							.rwx    = _rwx,
							.name   = { _filename.string() }
						};
					} else {
						out = {
							.fileno = (Genode::addr_t)this,
							.type   = Dirent_type::END,
							.rwx    = { },
							.name   = { }
						};
					}

					out_count = sizeof(Dirent);

					return READ_OK;
				}

				Write_result write(char const *, file_size, file_size &) override
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
			return (strlen(path) == (strlen(_filename.string()) + 1)) &&
			       (strcmp(&path[1], _filename.string()) == 0);
		}

	public:

		Single_file_system(Node_type   node_type,
		                   char const *type_name,
		                   Node_rwx    rwx,
		                   Xml_node    config)
		:
			_type(node_type), _rwx(rwx),
			_filename(config.attribute_value("name", Filename(type_name)))
		{ }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *) override
		{
			return Dataspace_capability();
		}

		void release(char const *, Dataspace_capability) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			out = Stat { };
			out.device = (Genode::addr_t)this;

			if (_root(path)) {
				out.type = Node_type::DIRECTORY;

			} else if (_single_file(path)) {
				out.type  = _type;
				out.rwx   = _rwx;
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

			try {
				*out_handle = new (alloc)
					Single_vfs_dir_handle(*this, *this, alloc,
					                      _type, _rwx, _filename);
				return OPENDIR_OK;
			}
			catch (Genode::Out_of_ram)  { return OPENDIR_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPENDIR_ERR_OUT_OF_CAPS; }
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Unlink_result unlink(char const *path) override
		{
			if (_single_file(path))
				return UNLINK_ERR_NO_PERM;

			return UNLINK_ERR_NO_ENTRY;
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

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_ERR_NO_PERM;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Single_vfs_handle *handle =
				static_cast<Single_vfs_handle*>(vfs_handle);

			if (handle)
				return handle->sync();

			return SYNC_ERR_INVALID;
		}
};

#endif /* _INCLUDE__VFS__SINGLE_FILE_SYSTEM_H_ */
