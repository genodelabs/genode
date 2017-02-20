/*
 * \brief  Representation of an open file
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__VFS_HANDLE_H_
#define _INCLUDE__VFS__VFS_HANDLE_H_

#include <vfs/directory_service.h>

namespace Vfs{
	class Vfs_handle;
	class File_io_service;
}


class Vfs::Vfs_handle
{
	private:

		Directory_service &_ds;
		File_io_service   &_fs;
		Genode::Allocator &_alloc;
		int                _status_flags;
		file_size          _seek = 0;

	public:

		/**
		 * Opaque handle context
		 */
		struct Context { };

		Context *context = nullptr;

		struct Guard
		{
			Vfs_handle *handle;

			Guard(Vfs_handle *handle) : handle(handle) { }

			~Guard()
			{
				if (handle)
					handle->_ds.close(handle);
			}
		};

		enum { STATUS_RDONLY = 0, STATUS_WRONLY = 1, STATUS_RDWR = 2 };

		Vfs_handle(Directory_service &ds,
		           File_io_service   &fs,
		           Genode::Allocator &alloc,
		           int                status_flags)
		:
			_ds(ds), _fs(fs),
			_alloc(alloc),
			_status_flags(status_flags)
		{ }

		virtual ~Vfs_handle() { }

		Directory_service &ds() { return _ds; }
		File_io_service   &fs() { return _fs; }
		Allocator      &alloc() { return _alloc; }

		int status_flags() const { return _status_flags; }

		void status_flags(int flags) { _status_flags = flags; }

		/**
		 * Return seek offset in bytes
		 */
		file_size seek() const { return _seek; }

		/**
		 * Set seek offset in bytes
		 */
		void seek(file_offset seek) { _seek = seek; }

		/**
		 * Advance seek offset by 'incr' bytes
		 */
		void advance_seek(file_size incr) { _seek += incr; }
};


#endif /* _INCLUDE__VFS__VFS_HANDLE_H_ */
