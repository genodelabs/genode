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
	class File_system;
	class Vfs_watch_handle;
}


class Vfs::Vfs_handle
{
	private:

		Directory_service &_ds;
		File_io_service   &_fs;
		Genode::Allocator &_alloc;
		int                _status_flags;
		file_size          _seek = 0;

		/*
		 * Noncopyable
		 */
		Vfs_handle(Vfs_handle const &);
		Vfs_handle &operator = (Vfs_handle const &);

	public:

		/**
		 * Opaque handle context
		 */
		struct Context : List<Context>::Element { };

		Context *context = nullptr;

		class Guard
		{
			private:

				/*
				 * Noncopyable
				 */
				Guard(Guard const &);
				Guard &operator = (Guard const &);

				Vfs_handle * const _handle;

			public:

				Guard(Vfs_handle *handle) : _handle(handle) { }

				~Guard()
				{
					if (_handle)
						_handle->close();
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

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { ds().close(this); }
};


class Vfs::Vfs_watch_handle
{
	public:

		/**
		 * Opaque handle context
		 */
		struct Context : List<Context>::Element { };

	private:

		Directory_service &_fs;
		Genode::Allocator &_alloc;
		Context           *_context = nullptr;

		/*
		 * Noncopyable
		 */
		Vfs_watch_handle(Vfs_watch_handle const &);
		Vfs_watch_handle &operator = (Vfs_watch_handle const &);

	public:

		Vfs_watch_handle(Directory_service &fs,
		                 Genode::Allocator &alloc)
		:
			_fs(fs), _alloc(alloc)
		{ }

		virtual ~Vfs_watch_handle() { }

		Directory_service &fs() { return _fs; }
		Allocator &alloc() { return _alloc; }
		virtual void context(Context *context) { _context = context; }
		Context *context() const { return _context; }

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { fs().close(this); }
};

#endif /* _INCLUDE__VFS__VFS_HANDLE_H_ */
