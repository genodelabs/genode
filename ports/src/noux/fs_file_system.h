/*
 * \brief  Adapter from Genode 'File_system' session to Noux file system
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FS_FILE_SYSTEM_H_
#define _NOUX__FS_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <file_system_session/connection.h>

namespace Noux {

	class Fs_file_system : public File_system
	{
		private:

			Lock _lock;

			Allocator_avl _fs_packet_alloc;

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

					Fs_vfs_handle(File_system *fs, int status_flags,
					              ::File_system::File_handle handle)
					: Vfs_handle(fs, fs, status_flags), _handle(handle)
					{ }

					~Fs_vfs_handle()
					{
						Fs_file_system *fs = static_cast<Fs_file_system *>(ds());
						fs->_fs.close(_handle);
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

			Dataspace_capability dataspace(char const *path)
			{
				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
			}

			bool stat(Sysio *sysio, char const *path)
			{
				::File_system::Status status;

				try {
					::File_system::Node_handle node = _fs.node(path);
					Fs_handle_guard node_guard(_fs, node);
					status = _fs.status(node);
				} catch (...) {
					PWRN("stat failed for path '%s'", path);
					return false;
				}

				sysio->stat_out.st.size = status.size;

				sysio->stat_out.st.mode = Sysio::STAT_MODE_FILE | 0777;

				if (status.is_symlink())
					sysio->stat_out.st.mode = Sysio::STAT_MODE_SYMLINK | 0777;

				if (status.is_directory())
					sysio->stat_out.st.mode = Sysio::STAT_MODE_DIRECTORY | 0777;

				sysio->stat_out.st.uid  = 0;
				sysio->stat_out.st.gid  = 0;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
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

				Sysio::Dirent_type type;
				switch (entry->type) {
				case Directory_entry::TYPE_DIRECTORY: type = Sysio::DIRENT_TYPE_DIRECTORY;  break;
				case Directory_entry::TYPE_FILE:      type = Sysio::DIRENT_TYPE_FILE;       break;
				case Directory_entry::TYPE_SYMLINK:   type = Sysio::DIRENT_TYPE_SYMLINK;    break;
				}

				sysio->dirent_out.entry.type   = type;
				sysio->dirent_out.entry.fileno = index + 1;

				strncpy(sysio->dirent_out.entry.name, entry->name,
				        sizeof(sysio->dirent_out.entry.name));

				source.release_packet(packet);

				return true;
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				Lock::Guard guard(_lock);

				Absolute_path dir_path(path);
				dir_path.strip_last_element();

				Absolute_path file_name(path);
				file_name.keep_only_last_element();

				try {
					::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
					Fs_handle_guard dir_guard(_fs, dir);

					_fs.unlink(dir, file_name.base() + 1);

				} catch (...) {
					sysio->error.unlink = Sysio::UNLINK_ERR_NO_ENTRY;
					return false;
				}
				return true;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
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
					sysio->error.unlink = Sysio::UNLINK_ERR_NO_ENTRY;
					return false;
				}
				return true;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				/*
				 * Canonicalize path (i.e., path must start with '/')
				 */
				Absolute_path abs_path(path);

				Sysio::Mkdir_error error = Sysio::MKDIR_ERR_NO_PERM;
				try {
					_fs.dir(abs_path.base(), true);
					return true;
				}
				catch (::File_system::Permission_denied)   { error = Sysio::MKDIR_ERR_NO_PERM; }
				catch (::File_system::Node_already_exists) { error = Sysio::MKDIR_ERR_EXISTS; }
				catch (::File_system::Lookup_failed)       { error = Sysio::MKDIR_ERR_NO_ENTRY; }
				catch (::File_system::Name_too_long)       { error = Sysio::MKDIR_ERR_NAME_TOO_LONG; }
				catch (::File_system::No_space)            { error = Sysio::MKDIR_ERR_NO_SPACE; }

				sysio->error.mkdir = error;
				return false;
			}

			size_t num_dirent(char const *path)
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

			bool is_directory(char const *path)
			{
				try {
					::File_system::Node_handle node = _fs.node(path);
					Fs_handle_guard node_guard(_fs, node);

					::File_system::Status status = _fs.status(node);

					return status.is_directory();
				}
				catch (...) { return false; }
			}

			char const *leaf_path(char const *path)
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

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				Lock::Guard guard(_lock);

				Absolute_path dir_path(path);
				dir_path.strip_last_element();

				Absolute_path file_name(path);
				file_name.keep_only_last_element();

				::File_system::Mode mode;

				switch (sysio->open_in.mode & Sysio::OPEN_MODE_ACCMODE) {
				default:                      mode = ::File_system::STAT_ONLY;  break;
				case Sysio::OPEN_MODE_RDONLY: mode = ::File_system::READ_ONLY;  break;
				case Sysio::OPEN_MODE_WRONLY: mode = ::File_system::WRITE_ONLY; break;
				case Sysio::OPEN_MODE_RDWR:   mode = ::File_system::READ_WRITE; break;
				}

				bool const create = sysio->open_in.mode & Sysio::OPEN_MODE_CREATE;

				if (create)
					PDBG("creation of file %s requested", file_name.base() + 1);

				Sysio::Open_error error = Sysio::OPEN_ERR_UNACCESSIBLE;

				try {
					::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
					Fs_handle_guard dir_guard(_fs, dir);

					::File_system::File_handle file = _fs.file(dir, file_name.base() + 1,
					                                           mode, create);
					return new (env()->heap()) Fs_vfs_handle(this, 0, file);
				}
				catch (::File_system::Permission_denied) {
					error = Sysio::OPEN_ERR_NO_PERM; }
				catch (::File_system::Invalid_handle) {
					error = Sysio::OPEN_ERR_NO_PERM; }
				catch (::File_system::Lookup_failed) { }

				sysio->error.open = error;
				return 0;
			}


			/***************************
			 ** File_system interface **
			 ***************************/

			char const *name() const { return "fs"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

				::File_system::Session::Tx::Source &source = *_fs.tx();

				size_t const max_packet_size = source.bulk_buffer_size() / 2;
				size_t const count = min(max_packet_size,
				                         min(sizeof(sysio->write_in.chunk),
				                             sysio->write_in.count));

				::File_system::Packet_descriptor
					packet(source.alloc_packet(count),
					       0,
					       handle->file_handle(),
					       ::File_system::Packet_descriptor::WRITE,
					       count,
					       handle->seek());

				memcpy(source.packet_content(packet), sysio->write_in.chunk, count);

				/* pass packet to server side */
				source.submit_packet(packet);
				source.get_acked_packet();

				sysio->write_out.count = count;

				/*
				 * XXX check if acked packet belongs to request,
				 *     needed for thread safety
				 */

				source.release_packet(packet);

				return true;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

				::File_system::Status status = _fs.status(handle->file_handle());
				size_t const file_size = status.size;

				size_t const file_bytes_left = file_size >= handle->seek()
				                             ? file_size  - handle->seek() : 0;

				::File_system::Session::Tx::Source &source = *_fs.tx();

				size_t const max_packet_size = source.bulk_buffer_size() / 2;
				size_t const count = min(max_packet_size,
				                         min(file_bytes_left,
				                             min(sizeof(sysio->read_out.chunk),
				                                 sysio->read_in.count)));

				::File_system::Packet_descriptor
					packet(source.alloc_packet(count),
					       0,
					       handle->file_handle(),
					       ::File_system::Packet_descriptor::READ,
					       count,
					       handle->seek());

				/* pass packet to server side */
				source.submit_packet(packet);
				source.get_acked_packet();

				memcpy(sysio->read_out.chunk, source.packet_content(packet), count);

				sysio->read_out.count = count;

				/*
				 * XXX check if acked packet belongs to request,
				 *     needed for thread safety
				 */

				source.release_packet(packet);
				return true;
			}
	};
}

#endif /* _NOUX__FS_FILE_SYSTEM_H_ */
