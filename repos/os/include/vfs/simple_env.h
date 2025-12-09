/*
 * \brief  VFS environment
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2018-04-04
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__SIMPLE_ENV_H_
#define _INCLUDE__VFS__SIMPLE_ENV_H_

#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <vfs/env.h>

namespace Genode::Vfs { struct Simple_env; }


class Genode::Vfs::Simple_env : public Env, private Env::Io, private Env::User
{
	private:

		Genode::Env &_env;
		Allocator   &_alloc;
		Env::User   &_user;

		using Deferred_wakeups = Remote_io::Deferred_wakeups;

		Deferred_wakeups _deferred_wakeups { };

		Global_file_system_factory _fs_factory { _alloc };

		Dir_file_system _root_dir;

	public:

		Simple_env(Genode::Env &env,
		           Allocator   &alloc,
		           Node  const &config,
		           Env::User   &user)
		:
			_env(env), _alloc(alloc), _user(user), _root_dir(*this, config, _fs_factory)
		{ }

		Simple_env(Genode::Env &env, Allocator &alloc, Node const &config)
		:
			Simple_env(env, alloc, config, *this)
		{ }

		void apply_config(Node const &config)
		{
			_root_dir.apply_config(config);
		}

		Genode::Env      &env()              override { return _env; }
		Allocator        &alloc()            override { return _alloc; }
		File_system      &root_dir()         override { return _root_dir; }
		Deferred_wakeups &deferred_wakeups() override { return _deferred_wakeups; }
		Env::Io          &io()               override { return *this; }
		Env::User        &user()             override { return _user; }

		/**
		 * Env::Io interface
		 */
		void commit() override { _deferred_wakeups.trigger(); }

		/**
		 * Env::Io interface
		 */
		void commit_and_wait() override
		{
			_deferred_wakeups.trigger();
			_env.ep().wait_and_dispatch_one_io_signal();
		}

		/**
		 * Env::User interface
		 *
		 * Fallback implementation used if no 'user' is specified at
		 * construction time.
		 */
		void wakeup_vfs_user() override { };
};

#endif /* _INCLUDE__VFS__SIMPLE_ENV_H_ */
