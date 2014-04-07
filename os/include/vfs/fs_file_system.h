/*
 * \brief  Adapter from Genode 'File_system' session to VFS
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__FS_FILE_SYSTEM_H_
#define _INCLUDE__VFS__FS_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <file_system_session/connection.h>

namespace Vfs { class Fs_file_system; }


class Vfs::Fs_file_system : public File_system
{
	enum { verbose = false };

	private:

		/*
		 * Lock used to serialize the interaction with the packet stream of the
		 * file-system session.
		 *
		 * XXX Once, we change the VFS file-system interface to use
		 *     asynchronous read/write operations, we can possibly remove it.
		 */
		Lock _lock;

		Genode::Allocator_avl _fs_packet_alloc;

		struct Label
		{
			enum { LABEL_MAX_LEN = 64 };
			char string[LABEL_MAX_LEN];

			Label(Xml_node config)
			{
				string[0] = 0;
				try { config.attribute("label").value(string, sizeof(string)); }
				catch (...) { }
			}
		} _label;

		::File_system::Connection _fs;

		class Fs_vfs_handle : public Vfs_handle
		{
			private:

				::File_system::File_handle const _handle;

			public:

				Fs_vfs_handle(File_system &fs, int status_flags,
				              ::File_system::File_handle handle)
				: Vfs_handle(fs, fs, status_flags), _handle(handle)
				{ }

				~Fs_vfs_handle()
				{
					Fs_file_system &fs = static_cast<Fs_file_system &>(ds());
					fs._fs.close(_handle);
				}

				::File_system::File_handle file_handle() const { return _handle; }
		};

		/**
		 * Helper for managing the lifetime of temporary open node handles
		 */
		struct Fs_handle_guard
		{
			::File_system::Session     &_fs;
			::File_system::Node_handle  _handle;

			Fs_handle_guard(::File_system::Session &fs,
			                ::File_system::Node_handle handle)
			: _fs(fs), _handle(handle) { }

			~Fs_handle_guard() { _fs.close(_handle); }
		};

		size_t _read(::File_system::Node_handle node_handle, void *buf,
		             size_t const count, size_t const seek_offset)
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();

			size_t const max_packet_size = source.bulk_buffer_size() / 2;
			size_t const clipped_count = min(max_packet_size, count);

			::File_system::Packet_descriptor const
				packet_in(source.alloc_packet(clipped_count),
				          0,
				          node_handle,
				          ::File_system::Packet_descriptor::READ,
				          clipped_count,
				          seek_offset);

			/* pass packet to server side */
			source.submit_packet(packet_in);

			/* obtain result packet descriptor with updated status info */
			::File_system::Packet_descriptor const
				packet_out = source.get_acked_packet();

			size_t const read_num_bytes = min(packet_out.length(), count);

			memcpy(buf, source.packet_content(packet_out), read_num_bytes);

			/*
			 * XXX check if acked packet belongs to request,
			 *     needed for thread safety
			 */

			source.release_packet(packet_out);

			return read_num_bytes;
		}

		size_t _write(::File_system::Node_handle node_handle,
		              const char *buf, size_t count, size_t seek_offset)
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();

			size_t const max_packet_size = source.bulk_buffer_size() / 2;
			count = min(max_packet_size, count);

			::File_system::Packet_descriptor
				packet(source.alloc_packet(count),
				       0,
				       node_handle,
				       ::File_system::Packet_descriptor::WRITE,
				       count,
				       seek_offset);

			memcpy(source.packet_content(packet), buf, count);

			/* pass packet to server side */
			source.submit_packet(packet);
			source.get_acked_packet();

			/*
			 * XXX check if acked packet belongs to request,
			 *     needed for thread safety
			 */

			source.release_packet(packet);

			return count;
		}

	public:

		/*
		 * XXX read label from config
		 */
		Fs_file_system(Xml_node config)
		:
			_fs_packet_alloc(env()->heap()),
			_label(config),
			_fs(_fs_packet_alloc, 128*1024, _label.string)
		{ }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			Lock::Guard guard(_lock);

			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			Ram_dataspace_capability ds_cap;
			char *local_addr = 0;

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(),
				                                        false);
				Fs_handle_guard dir_guard(_fs, dir);

				::File_system::File_handle file =
				    _fs.file(dir, file_name.base() + 1,
				             ::File_system::READ_ONLY, false);
				Fs_handle_guard file_guard(_fs, file);

				::File_system::Status status = _fs.status(file);

				Ram_dataspace_capability ds_cap =
				    env()->ram_session()->alloc(status.size);

				local_addr = env()->rm_session()->attach(ds_cap);

				::File_system::Session::Tx::Source &source = *_fs.tx();
				size_t const max_packet_size = source.bulk_buffer_size() / 2;

				for (size_t seek_offset = 0; seek_offset < status.size;
				     seek_offset += max_packet_size) {

					size_t const count = min(max_packet_size, status.size - seek_offset);

					::File_system::Packet_descriptor
						packet(source.alloc_packet(count),
							   0,
							   file,
							   ::File_system::Packet_descriptor::READ,
							   count,
							   seek_offset);

					/* pass packet to server side */
					source.submit_packet(packet);
					source.get_acked_packet();

					memcpy(local_addr + seek_offset, source.packet_content(packet), count);

					/*
					 * XXX check if acked packet belongs to request,
					 *     needed for thread safety
					 */

					source.release_packet(packet);
				}

				env()->rm_session()->detach(local_addr);

				return ds_cap;
			} catch(...) {
				env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
				return Dataspace_capability();
			}
		}

		void release(char const *path, Dataspace_capability ds_cap) override
		{
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			::File_system::Status status;

			try {
				::File_system::Node_handle node = _fs.node(path);
				Fs_handle_guard node_guard(_fs, node);
				status = _fs.status(node);
			} catch (...) {
				if (verbose)
					PDBG("stat failed for path '%s'", path);
				return STAT_ERR_NO_ENTRY;
			}

			memset(&out, 0, sizeof(out));

			out.size = status.size;
			out.mode = STAT_MODE_FILE | 0777;

			if (status.is_symlink())
				out.mode = STAT_MODE_SYMLINK | 0777;

			if (status.is_directory())
				out.mode = STAT_MODE_DIRECTORY | 0777;

			out.uid = 0;
			out.gid = 0;
			return STAT_OK;
		}

		Dirent_result dirent(char const *path, off_t index, Dirent &out) override
		{
			Lock::Guard guard(_lock);

			::File_system::Session::Tx::Source &source = *_fs.tx();

			if (strcmp(path, "") == 0)
				path = "/";

			::File_system::Dir_handle dir_handle = _fs.dir(path, false);
			Fs_handle_guard dir_guard(_fs, dir_handle);

			enum { DIRENT_SIZE = sizeof(::File_system::Directory_entry) };

			::File_system::Packet_descriptor
				packet(source.alloc_packet(DIRENT_SIZE),
				       0,
				       dir_handle,
				       ::File_system::Packet_descriptor::READ,
				       DIRENT_SIZE,
				       index*DIRENT_SIZE);

			/* pass packet to server side */
			source.submit_packet(packet);
			source.get_acked_packet();

			/*
			 * XXX check if acked packet belongs to request,
			 *     needed for thread safety
			 */

			typedef ::File_system::Directory_entry Directory_entry;

			/* copy-out payload into destination buffer */
			Directory_entry const *entry =
				(Directory_entry *)source.packet_content(packet);

			/*
			 * The default value has no meaning because the switch below
			 * assigns a value in each possible branch. But it is needed to
			 * keep the compiler happy.
			 */
			Dirent_type type = DIRENT_TYPE_END;

			switch (entry->type) {
			case Directory_entry::TYPE_DIRECTORY: type = DIRENT_TYPE_DIRECTORY; break;
			case Directory_entry::TYPE_FILE:      type = DIRENT_TYPE_FILE;      break;
			case Directory_entry::TYPE_SYMLINK:   type = DIRENT_TYPE_SYMLINK;   break;
			}

			out.type   = type;
			out.fileno = index + 1;

			strncpy(out.name, entry->name, sizeof(out.name));

			source.release_packet(packet);

			return DIRENT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
				Fs_handle_guard dir_guard(_fs, dir);

				_fs.unlink(dir, file_name.base() + 1);
			}
			catch (...) {
				return UNLINK_ERR_NO_ENTRY;
			}

			return UNLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf, size_t buf_size,
		                         size_t &out_len) override
		{
			/*
			 * Canonicalize path (i.e., path must start with '/')
			 */
			Absolute_path abs_path(path);
			abs_path.strip_last_element();

			Absolute_path symlink_name(path);
			symlink_name.keep_only_last_element();

			try {
				::File_system::Dir_handle dir_handle = _fs.dir(abs_path.base(), false);
				Fs_handle_guard from_dir_guard(_fs, dir_handle);

				::File_system::Symlink_handle symlink_handle =
				    _fs.symlink(dir_handle, symlink_name.base() + 1, false);
				Fs_handle_guard symlink_guard(_fs, symlink_handle);

				out_len = _read(symlink_handle, buf, buf_size, 0);

				return READLINK_OK;
			} catch (...) { }

			return READLINK_ERR_NO_ENTRY;
		}

		Rename_result rename(char const *from_path, char const *to_path) override
		{
			Absolute_path from_dir_path(from_path);
			from_dir_path.strip_last_element();

			Absolute_path from_file_name(from_path);
			from_file_name.keep_only_last_element();

			Absolute_path to_dir_path(to_path);
			to_dir_path.strip_last_element();

			Absolute_path to_file_name(to_path);
			to_file_name.keep_only_last_element();

			try {
				::File_system::Dir_handle from_dir = _fs.dir(from_dir_path.base(), false);
				Fs_handle_guard from_dir_guard(_fs, from_dir);

				::File_system::Dir_handle to_dir = _fs.dir(to_dir_path.base(), false);
				Fs_handle_guard to_dir_guard(_fs, to_dir);

				_fs.move(from_dir, from_file_name.base() + 1,
				         to_dir,   to_file_name.base() + 1);

			} catch (...) {
				return RENAME_ERR_NO_ENTRY; }

			return RENAME_OK;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			/*
			 * Canonicalize path (i.e., path must start with '/')
			 */
			Absolute_path abs_path(path);

			try {
				_fs.dir(abs_path.base(), true);
				return MKDIR_OK;
			}
			catch (::File_system::Permission_denied)   { return MKDIR_ERR_NO_PERM; }
			catch (::File_system::Node_already_exists) { return MKDIR_ERR_EXISTS; }
			catch (::File_system::Lookup_failed)       { return MKDIR_ERR_NO_ENTRY; }
			catch (::File_system::Name_too_long)       { return MKDIR_ERR_NAME_TOO_LONG; }
			catch (::File_system::No_space)            { return MKDIR_ERR_NO_SPACE; }

			return MKDIR_ERR_NO_PERM;
		}

		Symlink_result symlink(char const *from, char const *to) override
		{
			/*
			 * We write to the symlink via the packet stream. Hence we need
			 * to serialize with other packet-stream operations.
			 */
			Lock::Guard guard(_lock);

			/*
			 * Canonicalize path (i.e., path must start with '/')
			 */
			Absolute_path abs_path(to);
			abs_path.strip_last_element();

			Absolute_path symlink_name(to);
			symlink_name.keep_only_last_element();

			try {
				::File_system::Dir_handle dir_handle = _fs.dir(abs_path.base(), false);
				Fs_handle_guard from_dir_guard(_fs, dir_handle);

				::File_system::Symlink_handle symlink_handle =
				    _fs.symlink(dir_handle, symlink_name.base() + 1, true);
				Fs_handle_guard symlink_guard(_fs, symlink_handle);

				_write(symlink_handle, from, strlen(from) + 1, 0);
				return SYMLINK_OK;
			}
			catch (::File_system::Invalid_handle)      { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Node_already_exists) { return SYMLINK_ERR_EXISTS; }
			catch (::File_system::Invalid_name)        { return SYMLINK_ERR_NAME_TOO_LONG; }
			catch (::File_system::Lookup_failed)       { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Permission_denied)   { return SYMLINK_ERR_NO_PERM; }

			return SYMLINK_ERR_NO_ENTRY;
		}

		size_t num_dirent(char const *path) override
		{
			if (strcmp(path, "") == 0)
				path = "/";

			/*
			 * XXX handle exceptions
			 */
			::File_system::Node_handle node = _fs.node(path);
			Fs_handle_guard node_guard(_fs, node);

			::File_system::Status status = _fs.status(node);

			return status.size / sizeof(::File_system::Directory_entry);
		}

		bool is_directory(char const *path) override
		{
			try {
				::File_system::Node_handle node = _fs.node(path);
				Fs_handle_guard node_guard(_fs, node);

				::File_system::Status status = _fs.status(node);

				return status.is_directory();
			}
			catch (...) { return false; }
		}

		char const *leaf_path(char const *path) override
		{
			/* check if node at path exists within file system */
			try {
				::File_system::Node_handle node = _fs.node(path);
				_fs.close(node);
			}
			catch (...) {
				return 0; }

			return path;
		}

		Open_result open(char const *path, unsigned vfs_mode, Vfs_handle **out_handle) override
		{
			Lock::Guard guard(_lock);

			Absolute_path dir_path(path);
			dir_path.strip_last_element();

			Absolute_path file_name(path);
			file_name.keep_only_last_element();

			::File_system::Mode mode;

			switch (vfs_mode & OPEN_MODE_ACCMODE) {
			default:               mode = ::File_system::STAT_ONLY;  break;
			case OPEN_MODE_RDONLY: mode = ::File_system::READ_ONLY;  break;
			case OPEN_MODE_WRONLY: mode = ::File_system::WRITE_ONLY; break;
			case OPEN_MODE_RDWR:   mode = ::File_system::READ_WRITE; break;
			}

			bool const create = vfs_mode & OPEN_MODE_CREATE;

			if (create)
				if (verbose)
					PDBG("creation of file %s requested", file_name.base() + 1);

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
				Fs_handle_guard dir_guard(_fs, dir);

				::File_system::File_handle file = _fs.file(dir, file_name.base() + 1,
				                                           mode, create);

				*out_handle = new (env()->heap()) Fs_vfs_handle(*this, vfs_mode, file);
				return OPEN_OK;
			}
			catch (::File_system::Permission_denied)   { return OPEN_ERR_NO_PERM; }
			catch (::File_system::Invalid_handle)      { return OPEN_ERR_NO_PERM; }
			catch (::File_system::Lookup_failed)       { return OPEN_ERR_UNACCESSIBLE; }
			catch (::File_system::Node_already_exists) { return OPEN_ERR_EXISTS; }

			return OPEN_ERR_UNACCESSIBLE;
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name() { return "fs"; }

		void sync() override
		{
			_fs.sync();
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle, char const *buf, size_t buf_size,
		                   size_t &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			out_count = _write(handle->file_handle(), buf, buf_size, handle->seek());

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, size_t count,
		                 size_t &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			::File_system::Status status = _fs.status(handle->file_handle());
			size_t const file_size = status.size;

			size_t const file_bytes_left = file_size >= handle->seek()
			                             ? file_size  - handle->seek() : 0;

			count = min(count, file_bytes_left);

			out_count = _read(handle->file_handle(), dst, count, handle->seek());

			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, size_t len) override
		{
			Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			try {
				_fs.truncate(handle->file_handle(), len);
			} 
			catch (::File_system::Invalid_handle)    { return FTRUNCATE_ERR_NO_PERM; }
			catch (::File_system::Permission_denied) { return FTRUNCATE_ERR_NO_PERM; }

			return FTRUNCATE_OK;
		}
};

#endif /* _INCLUDE__VFS__FS_FILE_SYSTEM_H_ */
