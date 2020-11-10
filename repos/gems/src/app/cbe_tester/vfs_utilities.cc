/*
 * \brief  Utilities for a more convenient use of the VFS
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* CBE tester includes */
#include <vfs_utilities.h>

using namespace Genode;
using namespace Vfs;


/*****************************
 ** Vfs_io_response_handler **
 *****************************/

Vfs_io_response_handler::Vfs_io_response_handler(Genode::Signal_context_capability sigh)
:
	_sigh(sigh)
{ }


void Vfs_io_response_handler::read_ready_response() { }


void Vfs_io_response_handler::io_progress_response()
{
	Signal_transmitter(_sigh).submit();
}


/**********************
 ** Global functions **
 **********************/

Vfs::Vfs_handle &vfs_open(Vfs::Env                          &vfs_env,
                          Genode::String<128>                path,
                          Vfs::Directory_service::Open_mode  mode)
{
	Vfs_handle *handle { nullptr };
	Directory_service::Open_result const result {
		vfs_env.root_dir().open(
			path.string(), mode, &handle, vfs_env.alloc()) };

	if (result != Directory_service::Open_result::OPEN_OK) {

		error("failed to open file ", path.string());
		class Failed { };
		throw Failed { };
	}
	return *handle;
}


Vfs_handle &vfs_open_wo(Vfs::Env    &vfs_env,
                        String<128>  path)
{
	return vfs_open(vfs_env, path, Directory_service::OPEN_MODE_WRONLY);
}


Vfs::Vfs_handle &vfs_open_rw(Vfs::Env            &vfs_env,
                             Genode::String<128>  path)
{
	return vfs_open(vfs_env, path, Directory_service::OPEN_MODE_RDWR);
}
