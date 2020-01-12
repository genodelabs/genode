/*
 * \brief  Utility for writing data to a file via the Genode VFS library
 * \author Norman Feske
 * \date   2020-01-25
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NEW_FILE_H_
#define _NEW_FILE_H_

#include <os/vfs.h>

namespace Genode { class New_file; }

class Genode::New_file : Noncopyable
{
	public:

		struct Create_failed : Exception { };

	private:

		Entrypoint       &_ep;
		Allocator        &_alloc;
		Vfs::File_system &_fs;
		Vfs::Vfs_handle  &_handle;

		Vfs::Vfs_handle &_init_handle(Directory::Path const &path)
		{
			unsigned mode = Vfs::Directory_service::OPEN_MODE_WRONLY;

			Vfs::Directory_service::Stat stat { };
			if (_fs.stat(path.string(), stat) != Vfs::Directory_service::STAT_OK)
				mode |= Vfs::Directory_service::OPEN_MODE_CREATE;

			Vfs::Vfs_handle *handle_ptr = nullptr;
			Vfs::Directory_service::Open_result const res =
				_fs.open(path.string(), mode, &handle_ptr, _alloc);

			if (res != Vfs::Directory_service::OPEN_OK || (handle_ptr == nullptr)) {
				error("failed to create file '", path, "'");
				throw Create_failed();
			}

			handle_ptr->fs().ftruncate(handle_ptr, 0);

			return *handle_ptr;
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Open_failed
		 */
		New_file(Vfs::Env &env, Directory::Path const &path)
		:
			_ep(env.env().ep()), _alloc(env.alloc()), _fs(env.root_dir()),
			_handle(_init_handle(path))
		{ }

		~New_file()
		{
			while (_handle.fs().queue_sync(&_handle) == false)
				_ep.wait_and_dispatch_one_io_signal();

			for (bool sync_done = false; !sync_done; ) {

				switch (_handle.fs().complete_sync(&_handle)) {

				case Vfs::File_io_service::SYNC_QUEUED:
					break;

				case Vfs::File_io_service::SYNC_ERR_INVALID:
					warning("could not complete file sync operation");
					sync_done = true;
					break;

				case Vfs::File_io_service::SYNC_OK:
					sync_done = true;
					break;
				}

				if (!sync_done)
					_ep.wait_and_dispatch_one_io_signal();
			}
			_handle.ds().close(&_handle);
		}

		enum class Append_result { OK, WRITE_ERROR };

		Append_result append(char const *src, size_t size)
		{
			bool write_error = false;

			size_t remaining_bytes = size;

			while (remaining_bytes > 0 && !write_error) {

				bool stalled = false;

				try {
					Vfs::file_size out_count = 0;

					using Write_result = Vfs::File_io_service::Write_result;

					switch (_handle.fs().write(&_handle, src, remaining_bytes,
					                           out_count)) {

					case Write_result::WRITE_ERR_AGAIN:
					case Write_result::WRITE_ERR_WOULD_BLOCK:
						stalled = true;
						break;

					case Write_result::WRITE_ERR_INVALID:
					case Write_result::WRITE_ERR_IO:
					case Write_result::WRITE_ERR_INTERRUPT:
						write_error = true;
						break;

					case Write_result::WRITE_OK:
						out_count = min(remaining_bytes, out_count);
						remaining_bytes -= out_count;
						src             += out_count;
						_handle.advance_seek(out_count);
						break;
					};
				}
				catch (Vfs::File_io_service::Insufficient_buffer) {
					stalled = true; }

				if (stalled)
					_ep.wait_and_dispatch_one_io_signal();
			}
			return write_error ? Append_result::WRITE_ERROR
			                   : Append_result::OK;
		}
};

#endif /* _NEW_FILE_H_ */
