/*
 * \brief  Representation of an open file
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__VFS_HANDLE_H_
#define _INCLUDE__VFS__VFS_HANDLE_H_

#include <vfs/file_io_service.h>
#include <vfs/directory_service.h>

namespace Vfs { class Vfs_handle; }


class Vfs::Vfs_handle
{
	private:

		Directory_service &_ds;
		File_io_service   &_fs;
		int                _status_flags;
		file_size          _seek;

	public:

		struct Guard
		{
			Vfs_handle *handle;

			Guard(Vfs_handle *handle) : handle(handle) { }

			~Guard()
			{
				if (handle)
					Genode::destroy(Genode::env()->heap(), handle);
			}
		};

		enum { STATUS_RDONLY = 0, STATUS_WRONLY = 1, STATUS_RDWR = 2 };

		Vfs_handle(Directory_service &ds, File_io_service &fs, int status_flags)
		:
			_ds(ds), _fs(fs), _status_flags(status_flags), _seek(0)
		{ }

		virtual ~Vfs_handle() { }

		Directory_service &ds() { return _ds; }
		File_io_service   &fs() { return _fs; }

		int status_flags() const { return _status_flags; }

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
