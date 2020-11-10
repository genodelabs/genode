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

#ifndef _CBE_TESTER__VFS_UTILITIES_H_
#define _CBE_TESTER__VFS_UTILITIES_H_

/* Genode includes */
#include <vfs/vfs_handle.h>
#include <vfs/simple_env.h>

class Vfs_io_response_handler : public Vfs::Io_response_handler
{
	private:

		Genode::Signal_context_capability const _sigh;

	public:

		Vfs_io_response_handler(Genode::Signal_context_capability sigh);


		/******************************
		 ** Vfs::Io_response_handler **
		 ******************************/

		void read_ready_response() override;

		void io_progress_response() override;
};


Vfs::Vfs_handle &vfs_open(Vfs::Env                          &vfs_env,
                          Genode::String<128>                path,
                          Vfs::Directory_service::Open_mode  mode);


Vfs::Vfs_handle &vfs_open_wo(Vfs::Env            &vfs_env,
                             Genode::String<128>  path);


Vfs::Vfs_handle &vfs_open_rw(Vfs::Env            &vfs_env,
                             Genode::String<128>  path);

#endif /* _CBE_TESTER__VFS_UTILITIES_H_ */
