/*
 * \brief  Adapter from Genode 'File_system' session to VFS
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__FS_FILE_SYSTEM_H_
#define _INCLUDE__VFS__FS_FILE_SYSTEM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/id_space.h>
#include <file_system_session/connection.h>


namespace Vfs { class Fs_file_system; }


class Vfs::Fs_file_system : public File_system
{
	private:


		/*
		 * Lock used to serialize the interaction with the packet stream of the
		 * file-system session.
		 *
		 * XXX Once, we change the VFS file-system interface to use
		 *     asynchronous read/write operations, we can possibly remove it.
		 */
		Lock _lock;

		Genode::Env           &_env;
		Genode::Allocator_avl  _fs_packet_alloc;
		Io_response_handler   &_io_handler;

		typedef Genode::String<64> Label_string;
		Label_string _label;

		typedef Genode::String<::File_system::MAX_NAME_LEN> Root_string;
		Root_string _root;

		::File_system::Connection _fs;

		struct Fs_vfs_handle;

		struct Handle_space : Genode::Id_space<Fs_vfs_handle>
		{
			struct Id : Genode::Id_space<Fs_vfs_handle>::Id
			{
				Id(unsigned long v)              { value = v; }
				Id(::File_system::Node_handle h) { value = h.value; }
			};
		};

		Handle_space _handle_space;

		struct Handle_state
		{
			enum class Read_ready_state { IDLE, PENDING, READY };
			Read_ready_state read_ready_state = Read_ready_state::IDLE;

			enum class Queued_state { IDLE, QUEUED, ACK };
			Queued_state queued_read_state  = Queued_state::IDLE;
			Queued_state queued_write_state = Queued_state::IDLE;

			::File_system::Packet_descriptor queued_read_packet;
			::File_system::Packet_descriptor queued_write_packet;
		};

		struct Fs_vfs_handle : Vfs_handle, Handle_space::Element, Handle_state
		{
			Handle_space::Id const id;

			Fs_vfs_handle(File_system &fs, Allocator &alloc, int status_flags,
			              Handle_space &space, Handle_space::Id id)
			:
				Vfs_handle(fs, fs, alloc, status_flags),
				Handle_space::Element(*this, space, id),
				id(id)
			{ }

			::File_system::File_handle file_handle() const { return id.value; }
		};

		/**
		 * Helper for managing the lifetime of temporary open node handles
		 */
		struct Fs_handle_guard : Fs_vfs_handle
		{
			::File_system::Session     &_fs_session;
			::File_system::Node_handle  _fs_handle;

			Fs_handle_guard(File_system &fs,
			                ::File_system::Session &fs_session,
			                ::File_system::Node_handle fs_handle,
			                Handle_space &space)
			:
				Fs_vfs_handle(fs, *(Allocator*)nullptr, 0, space, Handle_space::Id(fs_handle)),
				_fs_session(fs_session), _fs_handle(fs_handle)
			{ }

			~Fs_handle_guard() { _fs_session.close(_fs_handle); }
		};

		struct Post_signal_hook : Genode::Entrypoint::Post_signal_hook
		{
			Genode::Entrypoint  &_ep;
			Io_response_handler &_io_handler;
			Vfs_handle::Context *_context = nullptr;

			Post_signal_hook(Genode::Entrypoint &ep,
			                 Io_response_handler &io_handler)
			: _ep(ep), _io_handler(io_handler) { }

			void arm(Vfs_handle::Context *context)
			{
				_context = context;
				_ep.schedule_post_signal_hook(this);
			}

			void function() override
			{
				/*
				 * XXX The current implementation executes the post signal hook
				 *     for the last armed context only. When changing this,
				 *     beware that the called handle_io_response() may change
				 *     this object in a signal handler.
				 */

				_io_handler.handle_io_response(_context);
				_context = nullptr;
			}
		};

		Post_signal_hook _post_signal_hook { _env.ep(), _io_handler };

		file_size _read(Fs_vfs_handle &handle, void *buf,
		                file_size const count, file_size const seek_offset)
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();
			using ::File_system::Packet_descriptor;

			file_size const max_packet_size = source.bulk_buffer_size() / 2;
			file_size const clipped_count = min(max_packet_size, count);

			/* XXX check if alloc_packet() and submit_packet() will succeed! */

			Packet_descriptor const packet_in(source.alloc_packet(clipped_count),
			                                  handle.file_handle(),
			                                  Packet_descriptor::READ,
			                                  clipped_count,
			                                  seek_offset);

			/* wait until packet was acknowledged */
			handle.queued_read_state = Handle_state::Queued_state::QUEUED;

			/* pass packet to server side */
			source.submit_packet(packet_in);

			while (handle.queued_read_state != Handle_state::Queued_state::ACK) {
				_env.ep().wait_and_dispatch_one_io_signal();
			}

			/* obtain result packet descriptor with updated status info */
			Packet_descriptor const packet_out = handle.queued_read_packet;

			handle.queued_read_state  = Handle_state::Queued_state::IDLE;
			handle.queued_read_packet = Packet_descriptor();

			if (!packet_out.succeeded()) {
				/* could be EOF or a real error */
				::File_system::Status status = _fs.status(handle.file_handle());
				if (seek_offset < status.size)
					Genode::warning("unexpected failure on file-system read");
			}

			file_size const read_num_bytes = min(packet_out.length(), count);

			memcpy(buf, source.packet_content(packet_out), read_num_bytes);

			source.release_packet(packet_out);

			return read_num_bytes;
		}

		file_size _write(Fs_vfs_handle &handle,
		                 const char *buf, file_size count, file_size seek_offset)
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();
			using ::File_system::Packet_descriptor;

			file_size const max_packet_size = source.bulk_buffer_size() / 2;
			count = min(max_packet_size, count);

			/* XXX check if alloc_packet() and submit_packet() will succeed! */

			Packet_descriptor packet_in(source.alloc_packet(count),
			                            handle.file_handle(),
			                            Packet_descriptor::WRITE,
			                            count,
			                            seek_offset);

			memcpy(source.packet_content(packet_in), buf, count);

			/* wait until packet was acknowledged */
			handle.queued_write_state = Handle_state::Queued_state::QUEUED;

			/* pass packet to server side */
			source.submit_packet(packet_in);

			while (handle.queued_write_state != Handle_state::Queued_state::ACK) {
				_env.ep().wait_and_dispatch_one_io_signal();
			}

			/* obtain result packet descriptor with updated status info */
			Packet_descriptor const packet_out = handle.queued_write_packet;

			handle.queued_write_state  = Handle_state::Queued_state::IDLE;
			handle.queued_write_packet = Packet_descriptor();

			file_size const write_num_bytes = min(packet_out.length(), count);

			source.release_packet(packet_out);

			return write_num_bytes;
		}

		void _handle_ack()
		{
			::File_system::Session::Tx::Source &source = *_fs.tx();
			using ::File_system::Packet_descriptor;

			while (source.ack_avail()) {

				Packet_descriptor const packet = source.get_acked_packet();

				Handle_space::Id const id(packet.handle());

				try {
					_handle_space.apply<Fs_vfs_handle>(id, [&] (Fs_vfs_handle &handle)
					{
						switch (packet.operation()) {
						case Packet_descriptor::READ_READY:
							handle.read_ready_state = Handle_state::Read_ready_state::READY;
							break;

						case Packet_descriptor::READ:
							handle.queued_read_packet = packet;
							handle.queued_read_state  = Handle_state::Queued_state::ACK;
							break;

						case Packet_descriptor::WRITE:
							handle.queued_write_packet = packet;
							handle.queued_write_state  = Handle_state::Queued_state::ACK;
							break;

						case Packet_descriptor::CONTENT_CHANGED:
							break;
						}

						_post_signal_hook.arm(handle.context);
					});
				} catch (Handle_space::Unknown_id) {
					Genode::warning("ack for unknown VFS handle"); }
			}
		}

		Genode::Io_signal_handler<Fs_file_system> _ack_handler {
			_env.ep(), *this, &Fs_file_system::_handle_ack };

	public:

		Fs_file_system(Genode::Env         &env,
		               Genode::Allocator   &alloc,
		               Genode::Xml_node     config,
		               Io_response_handler &io_handler)
		:
			_env(env),
			_fs_packet_alloc(&alloc),
			_io_handler(io_handler),
			_label(config.attribute_value("label", Label_string())),
			_root( config.attribute_value("root",  Root_string())),
			_fs(env, _fs_packet_alloc,
			    _label.string(), _root.string(),
			    config.attribute_value("writeable", true),
			    ::File_system::DEFAULT_TX_BUF_SIZE)
		{
			_fs.sigh_ack_avail(_ack_handler);
		}

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
				Fs_handle_guard dir_guard(*this, _fs, dir, _handle_space);

				::File_system::File_handle file =
				    _fs.file(dir, file_name.base() + 1,
				             ::File_system::READ_ONLY, false);
				Fs_handle_guard file_guard(*this, _fs, file, _handle_space);

				::File_system::Status status = _fs.status(file);

				Ram_dataspace_capability ds_cap =
				    _env.ram().alloc(status.size);

				local_addr = _env.rm().attach(ds_cap);

				::File_system::Session::Tx::Source &source = *_fs.tx();
				file_size const max_packet_size = source.bulk_buffer_size() / 2;

				for (file_size seek_offset = 0; seek_offset < status.size;
				     seek_offset += max_packet_size) {

					file_size const count = min(max_packet_size, status.size -
					                                             seek_offset);

					_read(file_guard, local_addr + seek_offset, count, seek_offset);
				}

				_env.rm().detach(local_addr);

				return ds_cap;
			} catch(...) {
				_env.rm().detach(local_addr);
				_env.ram().free(ds_cap);
				return Dataspace_capability();
			}
		}

		void release(char const *path, Dataspace_capability ds_cap) override
		{
			if (ds_cap.valid())
				_env.ram().free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			::File_system::Status status;

			try {
				::File_system::Node_handle node = _fs.node(path);
				Fs_handle_guard node_guard(*this, _fs, node, _handle_space);
				status = _fs.status(node);
			}
			catch (::File_system::Lookup_failed) { return STAT_ERR_NO_ENTRY; }
			catch (Genode::Out_of_ram)           { return STAT_ERR_NO_PERM;  }
			catch (Genode::Out_of_caps)          { return STAT_ERR_NO_PERM;  }

			out = Stat();

			out.size = status.size;
			out.mode = STAT_MODE_FILE | 0777;

			if (status.symlink())
				out.mode = STAT_MODE_SYMLINK | 0777;

			if (status.directory())
				out.mode = STAT_MODE_DIRECTORY | 0777;

			out.uid = 0;
			out.gid = 0;
			out.inode = status.inode;
			out.device = (Genode::addr_t)this;
			return STAT_OK;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &out) override
		{
			Lock::Guard guard(_lock);

			using ::File_system::Directory_entry;

			if (strcmp(path, "") == 0)
				path = "/";

			::File_system::Dir_handle dir_handle;
			try { dir_handle = _fs.dir(path, false); }
			catch (::File_system::Lookup_failed) { return DIRENT_ERR_INVALID_PATH; }
			catch (::File_system::Name_too_long) { return DIRENT_ERR_INVALID_PATH; }
			catch (...) { return DIRENT_ERR_NO_PERM; }

			Fs_handle_guard dir_guard(*this, _fs, dir_handle, _handle_space);
			Directory_entry entry;

			enum { DIRENT_SIZE = sizeof(Directory_entry) };

			_read(dir_guard, &entry, DIRENT_SIZE, index*DIRENT_SIZE);

			/*
			 * The default value has no meaning because the switch below
			 * assigns a value in each possible branch. But it is needed to
			 * keep the compiler happy.
			 */
			Dirent_type type = DIRENT_TYPE_END;

			/* copy-out payload into destination buffer */
			switch (entry.type) {
			case Directory_entry::TYPE_DIRECTORY: type = DIRENT_TYPE_DIRECTORY; break;
			case Directory_entry::TYPE_FILE:      type = DIRENT_TYPE_FILE;      break;
			case Directory_entry::TYPE_SYMLINK:   type = DIRENT_TYPE_SYMLINK;   break;
			}

			out.fileno = entry.inode;
			out.type   = type;
			strncpy(out.name, entry.name, sizeof(out.name));

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
				Fs_handle_guard dir_guard(*this, _fs, dir, _handle_space);

				_fs.unlink(dir, file_name.base() + 1);
			}
			catch (::File_system::Invalid_handle)    { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Invalid_name)      { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Lookup_failed)     { return UNLINK_ERR_NO_ENTRY;  }
			catch (::File_system::Not_empty)         { return UNLINK_ERR_NOT_EMPTY; }
			catch (::File_system::Permission_denied) { return UNLINK_ERR_NO_PERM;   }

			return UNLINK_OK;
		}

		Readlink_result readlink(char const *path, char *buf, file_size buf_size,
		                         file_size &out_len) override
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
				Fs_handle_guard from_dir_guard(*this, _fs, dir_handle, _handle_space);

				::File_system::Symlink_handle symlink_handle =
				    _fs.symlink(dir_handle, symlink_name.base() + 1, false);
				Fs_handle_guard symlink_guard(*this, _fs, symlink_handle, _handle_space);

				out_len = _read(symlink_guard, buf, buf_size, 0);

				return READLINK_OK;
			}
			catch (::File_system::Lookup_failed)  { return READLINK_ERR_NO_ENTRY; }
			catch (::File_system::Invalid_handle) { return READLINK_ERR_NO_ENTRY; }
			catch (...) { return READLINK_ERR_NO_PERM; }
		}

		Rename_result rename(char const *from_path, char const *to_path) override
		{
			if ((strcmp(from_path, to_path) == 0) && leaf_path(from_path))
				return RENAME_OK;

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
				Fs_handle_guard from_dir_guard(*this, _fs, from_dir, _handle_space);
				::File_system::Dir_handle to_dir = _fs.dir(to_dir_path.base(), false);
				Fs_handle_guard to_dir_guard(*this, _fs, to_dir, _handle_space);

				_fs.move(from_dir, from_file_name.base() + 1,
				         to_dir,   to_file_name.base() + 1);
			}
			catch (::File_system::Lookup_failed) { return RENAME_ERR_NO_ENTRY; }
			catch (...)                          { return RENAME_ERR_NO_PERM; }

			return RENAME_OK;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			/*
			 * Canonicalize path (i.e., path must start with '/')
			 */
			Absolute_path abs_path(path);

			try {
				_fs.close(_fs.dir(abs_path.base(), true));
			}
			catch (::File_system::Permission_denied)   { return MKDIR_ERR_NO_PERM; }
			catch (::File_system::Node_already_exists) { return MKDIR_ERR_EXISTS; }
			catch (::File_system::Lookup_failed)       { return MKDIR_ERR_NO_ENTRY; }
			catch (::File_system::Name_too_long)       { return MKDIR_ERR_NAME_TOO_LONG; }
			catch (::File_system::No_space)            { return MKDIR_ERR_NO_SPACE; }
			catch (::File_system::Out_of_ram)          { return MKDIR_ERR_NO_ENTRY; }
			catch (::File_system::Out_of_caps)         { return MKDIR_ERR_NO_ENTRY; }

			return MKDIR_OK;
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
				Fs_handle_guard from_dir_guard(*this, _fs, dir_handle, _handle_space);

				::File_system::Symlink_handle symlink_handle =
				    _fs.symlink(dir_handle, symlink_name.base() + 1, true);
				Fs_handle_guard symlink_guard(*this, _fs, symlink_handle, _handle_space);

				_write(symlink_guard, from, strlen(from) + 1, 0);
			}
			catch (::File_system::Invalid_handle)      { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Node_already_exists) { return SYMLINK_ERR_EXISTS;   }
			catch (::File_system::Invalid_name)        { return SYMLINK_ERR_NAME_TOO_LONG; }
			catch (::File_system::Lookup_failed)       { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Permission_denied)   { return SYMLINK_ERR_NO_PERM;  }
			catch (::File_system::No_space)            { return SYMLINK_ERR_NO_SPACE; }
			catch (::File_system::Out_of_ram)          { return SYMLINK_ERR_NO_ENTRY; }
			catch (::File_system::Out_of_caps)         { return SYMLINK_ERR_NO_ENTRY; }

			return SYMLINK_OK;
		}

		file_size num_dirent(char const *path) override
		{
			if (strcmp(path, "") == 0)
				path = "/";

			::File_system::Node_handle node;
			try { node = _fs.node(path); } catch (...) { return 0; }
			Fs_handle_guard node_guard(*this, _fs, node, _handle_space);

			::File_system::Status status = _fs.status(node);

			return status.size / sizeof(::File_system::Directory_entry);
		}

		bool directory(char const *path) override
		{
			try {
				::File_system::Node_handle node = _fs.node(path);
				Fs_handle_guard node_guard(*this, _fs, node, _handle_space);

				::File_system::Status status = _fs.status(node);

				return status.directory();
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
			catch (...) { return 0; }

			return path;
		}

		Open_result open(char const *path, unsigned vfs_mode, Vfs_handle **out_handle, Genode::Allocator& alloc) override
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

			try {
				::File_system::Dir_handle dir = _fs.dir(dir_path.base(), false);
				Fs_handle_guard dir_guard(*this, _fs, dir, _handle_space);

				::File_system::File_handle file = _fs.file(dir, file_name.base() + 1,
				                                           mode, create);

				Handle_space::Id id { file };
				*out_handle = new (alloc)
					Fs_vfs_handle(*this, alloc, vfs_mode, _handle_space, id);
			}
			catch (::File_system::Lookup_failed)       { return OPEN_ERR_UNACCESSIBLE;  }
			catch (::File_system::Permission_denied)   { return OPEN_ERR_NO_PERM;       }
			catch (::File_system::Invalid_handle)      { return OPEN_ERR_UNACCESSIBLE;  }
			catch (::File_system::Node_already_exists) { return OPEN_ERR_EXISTS;        }
			catch (::File_system::Invalid_name)        { return OPEN_ERR_NAME_TOO_LONG; }
			catch (::File_system::Name_too_long)       { return OPEN_ERR_NAME_TOO_LONG; }
			catch (::File_system::No_space)            { return OPEN_ERR_NO_SPACE;      }
			catch (::File_system::Out_of_ram)          { return OPEN_ERR_NO_PERM;       }
			catch (::File_system::Out_of_caps)         { return OPEN_ERR_NO_PERM;       }

			return OPEN_OK;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (!vfs_handle) return;

			Lock::Guard guard(_lock);

			Fs_vfs_handle *fs_handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			if (fs_handle) {
				_fs.close(fs_handle->file_handle());
				destroy(fs_handle->alloc(), fs_handle);
			}
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name()   { return "fs"; }
		char const *type() override { return "fs"; }

		void sync(char const *path) override
		{
			try {
				::File_system::Node_handle node = _fs.node(path);
				_fs.sync(node);
				_fs.close(node);
			} catch (...) { }
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle, char const *buf,
		                   file_size buf_size, file_size &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle &handle = static_cast<Fs_vfs_handle &>(*vfs_handle);

			out_count = _write(handle, buf, buf_size, handle.seek());

			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle &handle = static_cast<Fs_vfs_handle &>(*vfs_handle);

			/* reset the ready_ready state */
			handle.read_ready_state = Handle_state::Read_ready_state::IDLE;

			out_count = _read(handle, dst, count, handle.seek());

			return READ_OK;
		}

		bool queue_read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                Read_result &out_result, file_size &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			if (handle->queued_read_state != Handle_state::Queued_state::IDLE)
				return false;

			::File_system::Session::Tx::Source &source = *_fs.tx();

			/* if not ready to submit suggest retry */
			if (!source.ready_to_submit()) return false;

			file_size const max_packet_size = source.bulk_buffer_size() / 2;
			file_size const clipped_count = min(max_packet_size, count);

			::File_system::Packet_descriptor p;
			try {
				p = source.alloc_packet(clipped_count);
			} catch (::File_system::Session::Tx::Source::Packet_alloc_failed) {
				return false;
			}

			::File_system::Packet_descriptor const
				packet(p, handle->file_handle(),
				       ::File_system::Packet_descriptor::READ,
				       clipped_count, handle->seek());

			handle->read_ready_state  = Handle_state::Read_ready_state::IDLE;
			handle->queued_read_state = Handle_state::Queued_state::QUEUED;

			out_result = READ_QUEUED;

			/* pass packet to server side */
			source.submit_packet(packet);

			return true;
		}

		Read_result complete_read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                          file_size &out_count) override
		{
			Lock::Guard guard(_lock);

			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			if (handle->queued_read_state != Handle_state::Queued_state::ACK)
				return READ_QUEUED;

			/* obtain result packet descriptor with updated status info */
			::File_system::Packet_descriptor const
				packet = handle->queued_read_packet;

			file_size const read_num_bytes = min(packet.length(), count);

			::File_system::Session::Tx::Source &source = *_fs.tx();

			memcpy(dst, source.packet_content(packet), read_num_bytes);

			handle->queued_read_state  = Handle_state::Queued_state::IDLE;
			handle->queued_read_packet = ::File_system::Packet_descriptor();

			out_count  = read_num_bytes;

			source.release_packet(packet);

			return READ_OK;
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			return handle->read_ready_state == Handle_state::Read_ready_state::READY;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			Fs_vfs_handle *handle = static_cast<Fs_vfs_handle *>(vfs_handle);
			if (handle->read_ready_state != Handle_state::Read_ready_state::IDLE)
				return true;

			::File_system::Session::Tx::Source &source = *_fs.tx();

			/* if not ready to submit suggest retry */
			if (!source.ready_to_submit()) return false;

			using ::File_system::Packet_descriptor;

			Packet_descriptor packet(Packet_descriptor(),
			                         handle->file_handle(),
			                         Packet_descriptor::READ_READY,
			                         0, 0);

			handle->read_ready_state = Handle_state::Read_ready_state::PENDING;

			source.submit_packet(packet);

			/*
			 * When the packet is acknowledged the application is notified via
			 * Io_response_handler::handle_io_response().
			 */
			return true;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Fs_vfs_handle const *handle = static_cast<Fs_vfs_handle *>(vfs_handle);

			try {
				_fs.truncate(handle->file_handle(), len);
			}
			catch (::File_system::Invalid_handle)    { return FTRUNCATE_ERR_NO_PERM; }
			catch (::File_system::Permission_denied) { return FTRUNCATE_ERR_NO_PERM; }
			catch (::File_system::No_space)          { return FTRUNCATE_ERR_NO_SPACE; }

			return FTRUNCATE_OK;
		}
};

#endif /* _INCLUDE__VFS__FS_FILE_SYSTEM_H_ */
