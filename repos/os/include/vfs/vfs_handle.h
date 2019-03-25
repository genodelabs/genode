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
	struct Io_response_handler;
	struct Watch_response_handler;
	class Vfs_handle;
	class Vfs_watch_handle;
	class File_io_service;
	class File_system;
}


/**
 * Object for encapsulating application-level
 * response to VFS I/O
 *
 * These responses should be assumed to be called
 * during I/O signal dispatch.
 */
struct Vfs::Io_response_handler : Genode::Interface
{
	/**
	 * Respond to a resource becoming readable
	 */
	virtual void read_ready_response() = 0;

	/**
	 * Respond to complete pending I/O
	 */
	virtual void io_progress_response() = 0;
};


/**
 * Object for encapsulating application-level
 * handlers of VFS responses.
 *
 * This response should be assumed to be called
 * during I/O signal dispatch.
 */
struct Vfs::Watch_response_handler : Genode::Interface
{
	virtual void watch_response() = 0;
};


class Vfs::Vfs_handle
{
	private:

		Directory_service   &_ds;
		File_io_service     &_fs;
		Genode::Allocator   &_alloc;
		Io_response_handler *_handler = nullptr;
		file_size            _seek = 0;
		int                  _status_flags;

		/*
		 * Noncopyable
		 */
		Vfs_handle(Vfs_handle const &);
		Vfs_handle &operator = (Vfs_handle const &);

	public:

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
		 * Set response handler, unset with nullptr
		 */
		virtual void handler(Io_response_handler *handler)
		{
			_handler = handler;
		}

		/**
		 * Apply to response handler if present
		 *
		 * XXX: may not be necesarry if the method above is virtual.
		 */
		template <typename FUNC>
		void apply_handler(FUNC const &func) const {
			if (_handler) func(*_handler); }

		/**
		 * Notify application through response handler
		 */
		void read_ready_response() {
			if (_handler) _handler->read_ready_response(); }

		/**
		 * Notify application through response handler
		 */
		void io_progress_response() {
			if (_handler) _handler->io_progress_response(); }

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { ds().close(this); }
};


class Vfs::Vfs_watch_handle
{
	private:

		Directory_service      &_fs;
		Genode::Allocator      &_alloc;
		Watch_response_handler *_handler = nullptr;

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

		/**
		 * Set response handler, unset with nullptr
		 */
		virtual void handler(Watch_response_handler *handler) {
			_handler = handler; }

		/**
		 * Notify application through response handler
		 */
		void watch_response()
		{
			if (_handler)
				_handler->watch_response();
		}

		/**
		 * Close handle at backing file-system.
		 *
		 * This leaves the handle pointer in an invalid and unsafe state.
		 */
		inline void close() { fs().close(this); }
};

#endif /* _INCLUDE__VFS__VFS_HANDLE_H_ */
