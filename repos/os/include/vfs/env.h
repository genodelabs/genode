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
		/**
		 * Trigger the deferred wakeup of remote peers
		 */
		virtual void commit() = 0;

		/**
		 * Wakeup remote peers and wait for I/O progress
		 *
		 * This method is intended for implementing synchronous I/O.
		 */
		virtual void commit_and_wait() = 0;
	};

	virtual Io &io() = 0;

	/**
	 * Interface for notifying the VFS user about possible progress
	 *
	 * This interface allows VFS plugins to prompt the potential unblocking of
	 * the VFS user, e.g., continuing a write operation that was stalled
	 * because of a saturated I/O buffer.
	 */
	struct User : Interface, Genode::Noncopyable
	{
		/**
		 * Called whenever the VFS observes I/O
		 *
		 * Note that this method is usually called from the context of an
		 * I/O signal handler. Hence, it must never execute application-level
		 * code. Otherwise, unexpected nesting of application-level code might
		 * occur, in particular if the application performs synchronous I/O
		 * via 'wait_and_dispatch_one_io_signal()'.
		 *
		 * There are two recommended ways to safely implement this interface:
		 *
		 * The first option is to record the occurence of I/O for a later
		 * application-level response by modifying a state variable.
		 *
		 * The second way is reflecting the condition to an application-level
		 * signal handler by calling 'Signal_handler<T>::local_submit()'.
		 * This way, the application-level signal handler is executed once
		 * the component goes idle next time. This handler can then safely
		 * enter application-level code.
		 */
		virtual void wakeup_vfs_user() = 0;
	};

	virtual User &user() = 0;
};

#endif /* _INCLUDE__VFS__ENV_H_ */
