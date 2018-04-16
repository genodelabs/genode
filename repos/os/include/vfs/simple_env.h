/*
 * \brief  Cross-plugin VFS environment
 * \author Emery Hemingway
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

class Vfs::Simple_env : public Vfs::Env
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		struct Io_response_dummy : Vfs::Io_response_handler {
			void handle_io_response(Vfs::Vfs_handle::Context*) override { }
		} _io_dummy { };

		struct Watch_response_dummy : Vfs::Watch_response_handler {
			void handle_watch_response(Vfs::Vfs_watch_handle::Context*) override { }
		} _watch_dummy { };

		Vfs::Global_file_system_factory _fs_factory { _alloc };

		Vfs::Dir_file_system _root_dir;

	public:

		Simple_env(Genode::Env       &env,
		           Genode::Allocator &alloc,
		           Genode::Xml_node   config)
		:
			_env(env), _alloc(alloc), _root_dir(*this, config, _fs_factory)
		{ }

		void apply_config(Genode::Xml_node const &config)
		{
			_root_dir.apply_config(config);
		}

		Genode::Env       &env()       override { return _env; }
		Genode::Allocator &alloc()     override { return _alloc; }
		Vfs::File_system  &root_dir()  override { return _root_dir; }

		Vfs::Io_response_handler    &io_handler()    override { return    _io_dummy; }
		Vfs::Watch_response_handler &watch_handler() override { return _watch_dummy; }
};

#endif /* _INCLUDE__VFS__SIMPLE_ENV_H_ */
