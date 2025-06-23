/*
 * \brief  file descriptor allocator interface
 * \author Christian Prochaska 
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_PLUGIN__FD_ALLOC_H_
#define _LIBC_PLUGIN__FD_ALLOC_H_

/* Genode includes */
#include <base/mutex.h>
#include <base/log.h>
#include <base/node.h>
#include <os/path.h>
#include <base/allocator.h>
#include <base/id_space.h>
#include <util/bit_allocator.h>
#include <vfs/vfs_handle.h>

/* libc includes */
#include <stdlib.h>
#include <string.h>

/* libc-internal includes */
#include <internal/plugin.h>

enum { MAX_NUM_FDS = 1024 };

namespace Libc {

	/**
	 * Plugin-specific file-descriptor context
	 */
	struct Plugin_context { virtual ~Plugin_context() { } };

	enum { ANY_FD = -1 };

	struct File_descriptor;

	class File_descriptor_allocator;
}


struct Libc::File_descriptor
{
	Genode::Mutex mutex { };

	using Id_space = Genode::Id_space<File_descriptor>;
	Id_space::Element _elem;

	int const libc_fd = _elem.id().value;

	char const *fd_path = nullptr;  /* for 'fchdir', 'fstat' */

	Plugin         *plugin;
	Plugin_context *context;

	struct Aio_handle
	{
		enum class State { INVALID, QUEUED, COMPLETE };

		Vfs::Vfs_handle *vfs_handle = nullptr;
		State            state      = State::INVALID;

		bool used = false;

		::size_t count  = 0;
		::off_t  offset = 0;

		void with_vfs_handle(auto const &vfs_handle_fn)
		{
			if (vfs_handle)
				vfs_handle_fn(*vfs_handle);
		}

		void reset()
		{
			used   = false;
			count  = 0;
			offset = 0;
			state  = State::INVALID;
		}
	};

	static constexpr unsigned MAX_VFS_HANDLES_PER_FD = 64;
	Aio_handle _aio_handles[MAX_VFS_HANDLES_PER_FD] { };

	void any_unused_aio_handle(auto const &fn)
	{
		for (unsigned i = 0; i < MAX_VFS_HANDLES_PER_FD; i++)
			if (!_aio_handles[i].used) {
				fn(_aio_handles[i]);
				break;
			}
	}

	void _close_aio_handles()
	{
		for (auto & handle : _aio_handles)
			if (handle.vfs_handle) {
				handle.vfs_handle->close();
				handle.vfs_handle = nullptr;
			}
	}

	struct Aio_job
	{
		enum class State { FREE, PENDING, IN_PROGRESS, COMPLETE };

		const struct aiocb *iocb = nullptr;

		Aio_handle *handle = nullptr;
		ssize_t     result = -1;
		int         error  = 0;
		State       state  = State::FREE;

		void acquire_handle(Aio_handle &aio_handle)
		{
			handle       = &aio_handle;
			handle->used = true;
		}

		void release_handle()
		{
			if (!handle)
				return;

			handle->reset();
			handle = nullptr;
		}

		void with_aio_handle(auto const &handle_fn)
		{
			if (handle)
				handle_fn(*handle);
		}

		void free()
		{
			handle = nullptr;
			iocb   = nullptr;
			error  = 0;
			result = -1;
			state  = State::FREE;
		}
	};

	static constexpr unsigned MAX_AIOCB_PER_FD = MAX_VFS_HANDLES_PER_FD;
	Aio_job _aio_jobs[MAX_AIOCB_PER_FD] { };

	void for_each_aio_job(Aio_job::State state, auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (_aio_jobs[i].state == state)
				fn(_aio_jobs[i]);
	}

	bool any_free_aio_job(auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (_aio_jobs[i].state == Aio_job::State::FREE) {
				fn(_aio_jobs[i]);
				return true;
			}

		return false;
	}

	void apply_lio(struct aiocb const *iocb, auto const &fn)
	{
		for (unsigned i = 0; i < MAX_AIOCB_PER_FD; i++)
			if (iocb == _aio_jobs[i].iocb)
				fn(_aio_jobs[i]);
	}

	unsigned lio_list_completed = 0;
	unsigned lio_list_queued    = 0;

	int  flags    = 0;  /* for 'fcntl' */
	bool cloexec  = 0;  /* for 'fcntl' */
	bool modified = false;

	File_descriptor(Id_space &id_space, Plugin &plugin, Plugin_context &context,
	                Id_space::Id id)
	: _elem(*this, id_space, id), plugin(&plugin), context(&context) { }

	~File_descriptor()
	{
		_close_aio_handles();
	}

	void path(char const *newpath);
};


class Libc::File_descriptor_allocator
{
	private:

		Genode::Mutex _mutex;

		Genode::Allocator &_alloc;

		using Id_space = File_descriptor::Id_space;

		Id_space _id_space;

		using Id_bit_alloc = Genode::Bit_allocator<MAX_NUM_FDS>;

		Id_bit_alloc _id_allocator;

	public:

		/**
		 * Constructor
		 */
		File_descriptor_allocator(Genode::Allocator &_alloc);

		/**
		 * Allocate file descriptor
		 */
		File_descriptor *alloc(Plugin *plugin, Plugin_context *context, int libc_fd = -1);

		/**
		 * Release file descriptor
		 */
		void free(File_descriptor *fdo);

		/**
		 * Prevent the use of the specified file descriptor
		 */
		void preserve(int libc_fd);

		File_descriptor *find_by_libc_fd(int libc_fd);

		/**
		 * Return any file descriptor with close-on-execve flag set
		 *
		 * \return pointer to file descriptor, or
		 *         nullptr is no such file descriptor exists
		 */
		File_descriptor *any_cloexec_libc_fd();

		/**
		 * Update seek state of file descriptor with append flag set.
		 */
		void update_append_libc_fds();

		/**
		 * Return file-descriptor ID of any open file, or -1 if no file is
		 * open
		 */
		int any_open_fd();

		void generate_info(Genode::Generator &);
};


#endif /* _LIBC_PLUGIN__FD_ALLOC_H_ */
