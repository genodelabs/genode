/*
 * \brief  Minimal file system for GPU session
 * \author Sebastian Sumpf
 * \date   2021-10-14
 *
 * The file system only handles completion signals of the GPU session in order
 * to work from non-EP threads (i.e., pthreads) in libc components. A read
 * returns only in case a completion signal has been delivered since the
 * previous call to read.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <gpu_session/connection.h>
#include <os/vfs.h>
#include <util/xml_generator.h>
#include <vfs/single_file_system.h>

#include "vfs_gpu.h"

namespace Vfs_gpu
{
	using namespace Vfs;
	using namespace Genode;

	struct File_system;
}

struct Vfs_gpu::File_system : Single_file_system
{
	struct Gpu_vfs_handle : Single_vfs_handle
	{
		bool             _complete { false };
		Genode::Env     &_env;
		Gpu::Connection  _gpu_session { _env };

		Genode::Io_signal_handler<Gpu_vfs_handle> _completion_sigh {
			_env.ep(), *this, &Gpu_vfs_handle::_handle_completion };

		using Id_space = Genode::Id_space<Gpu_vfs_handle>;
		Id_space::Element const _elem;

		void _handle_completion()
		{
			_complete = true;
			io_progress_response();
		}

		Gpu_vfs_handle(Genode::Env &env,
		               Directory_service &ds,
		               File_io_service   &fs,
		               Genode::Allocator &alloc,
		               Id_space &space)
		: Single_vfs_handle(ds, fs, alloc, 0),
		  _env(env), _elem(*this, space)
		{
			_gpu_session.completion_sigh(_completion_sigh);
		}

		Read_result read(char *dst, file_size /* count */,
		                 file_size &out_count) override
		{
			if (!_complete) return READ_QUEUED;

			_complete = false;
			dst[0]    = 1;
			out_count = 1;

			return READ_OK;
		}

		Write_result write(char const *, file_size, file_size &) override
		{
			return WRITE_ERR_IO;
		}

		bool read_ready()  const override { return _complete; }
		bool write_ready() const override { return true; }

		Id_space::Id id() const { return _elem.id(); }
	};

	Vfs::Env &_env;

	typedef String<32> Config;

	Id_space<Gpu_vfs_handle>     _handle_space { };
	Id_space<Gpu_vfs_handle>::Id _last_id { .value = ~0ul };

	File_system(Vfs::Env &env, Xml_node config)
	:
	  Single_file_system(Node_type::CONTINUOUS_FILE,
	                     type_name(),
	                     Node_rwx::ro(),
	                     config),
	  _env(env)
	{ }

	Open_result open(char const  *path, unsigned,
	                 Vfs::Vfs_handle **out_handle,
	                 Allocator   &alloc) override
	{
		if (!_single_file(path))
			return OPEN_ERR_UNACCESSIBLE;

		try {
			Gpu_vfs_handle *handle  = new (alloc)
				Gpu_vfs_handle(_env.env(), *this, *this, alloc, _handle_space);

			_last_id = handle->id();
			*out_handle = handle;

			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
	}

	Stat_result stat(char const *path, Stat &out) override
	{
		if (!_single_file(path))
			return STAT_ERR_NO_ENTRY;

		out.inode = (unsigned long)_last_id.value;

		return STAT_OK;
	}

	static char const *type_name() { return "gpu"; }
	char const *type() override { return type_name(); }
};


static Vfs_gpu::File_system *_fs { nullptr };

/**
 * XXX: return GPU session for given ID, retrieved by 'stat->inode'.
 * This function is used, for example, by libdrm
 */
Gpu::Connection *vfs_gpu_connection(unsigned long id)
{
	if (!_fs) return nullptr;

	using Gpu_vfs_handle = Vfs_gpu::File_system::Gpu_vfs_handle;
	using Id_space = Genode::Id_space<Gpu_vfs_handle>;

	try {
		return _fs->_handle_space.apply<Gpu_vfs_handle>(
			Id_space::Id { .value = id },
			[] (Gpu_vfs_handle &handle)
			{
				return &handle._gpu_session;
			}
		);
	} catch (...) { }

	return nullptr;
}


static Vfs::Env *_env { nullptr };

Genode::Env *vfs_gpu_env()
{
	return _env ? &_env->env() : nullptr;
}

/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			_env = &vfs_env;
			try {
				_fs = new (vfs_env.alloc()) Vfs_gpu::File_system(vfs_env, node);
				return _fs;
			}
			catch (...) { Genode::error("could not create 'gpu_fs' "); }
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}
