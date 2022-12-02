/*
 * \brief  Cross-plugin VFS environment
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2018-04-02
 */

/*
 * Copyright (C) 2018-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__ENV_H_
#define _INCLUDE__VFS__ENV_H_

#include <vfs/file_system.h>
#include <vfs/remote_io.h>
#include <base/allocator.h>
#include <base/env.h>

namespace Vfs { struct Env; }

struct Vfs::Env : Interface
{
	virtual Genode::Env &env() = 0;

	/**
	 * Allocator for creating stuctures shared across open VFS handles
	 */
	virtual Genode::Allocator &alloc() = 0;

	/**
	 * VFS root file system
	 */
	virtual File_system &root_dir() = 0;

	/**
	 * Registry of deferred wakeups for plugins interacting with remote peers
	 */
	virtual Remote_io::Deferred_wakeups &deferred_wakeups() = 0;

	/**
	 * Interface tailored for triggering and waiting for I/O
	 */
	struct Io : Interface, Genode::Noncopyable
	{
		virtual void progress() = 0;
	};

	virtual Io &io() = 0;
};

#endif /* _INCLUDE__VFS__ENV_H_ */
