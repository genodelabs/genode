/*
 * \brief  Cross-plugin VFS environment
 * \author Emery Hemingway
 * \date   2018-04-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__ENV_H_
#define _INCLUDE__VFS__ENV_H_

#include <vfs/file_system.h>
#include <base/allocator.h>
#include <base/env.h>

namespace Vfs { struct Env; }

struct Vfs::Env : Interface
{
	virtual Genode::Env &env() = 0;

	/**
	 * Allocator for creating stuctures shared
	 * across open VFS handles.
	 */
	virtual Genode::Allocator &alloc() = 0;

	/**
	 * VFS root file-system
	 */
	virtual File_system &root_dir() = 0;

	virtual Io_response_handler &io_handler() = 0;

	virtual Watch_response_handler &watch_handler() = 0;
};

#endif /* _INCLUDE__VFS__ENV_H_ */
