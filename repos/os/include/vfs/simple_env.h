/*
 * \brief  Cross-plugin VFS environment
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

namespace Vfs { struct Simple_env; }


class Vfs::Simple_env : public Vfs::Env, private Vfs::Env::Io, private Vfs::Env::User
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;
		Vfs::Env::User    &_user;

		Global_file_system_factory _fs_factory { _alloc };

		Dir_file_system _root_dir;

		using Deferred_wakeups = Remote_io::Deferred_wakeups;

		Deferred_wakeups _deferred_wakeups { };

	public:

		Simple_env(Genode::Env       &env,
		           Genode::Allocator &alloc,
		           Genode::Xml_node   config,
		           Vfs::Env::User    &user)
		:
			_env(env), _alloc(alloc), _user(user), _root_dir(*this, config, _fs_factory)
		{ }

		Simple_env(Genode::Env &env, Genode::Allocator &alloc, Genode::Xml_node config)
		:
			Simple_env(env, alloc, config, *this)
		{ }

		void apply_config(Genode::Xml_node const &config)
		{
			_root_dir.apply_config(config);
		}

		Genode::Env       &env()              override { return _env; }
		Genode::Allocator &alloc()            override { return _alloc; }
		Vfs::File_system  &root_dir()         override { return _root_dir; }
		Deferred_wakeups  &deferred_wakeups() override { return _deferred_wakeups; }
		Vfs::Env::Io      &io()               override { return *this; }
		Vfs::Env::User    &user()             override { return _user; }

		/**
		 * Vfs::Env::Io interface
		 */
		void commit() override { _deferred_wakeups.trigger(); }

		/**
		 * Vfs::Env::Io interface
		 */
		void commit_and_wait() override
		{
			_deferred_wakeups.trigger();
			_env.ep().wait_and_dispatch_one_io_signal();
		}

		/**
		 * Vfs::Env::User interface
		 *
		 * Fallback implementation used if no 'user' is specified at
		 * construction time.
		 */
		void wakeup_vfs_user() override { };
};

#endif /* _INCLUDE__VFS__SIMPLE_ENV_H_ */
