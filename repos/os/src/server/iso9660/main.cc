/*
 * \brief  Rom-session server for ISO file systems
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <dataspace/client.h>
#include <root/component.h>
#include <util/avl_string.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_label.h>
#include <block_session/connection.h>
#include <rom_session/rom_session.h>

/* local includes */
#include "iso9660.h"

using namespace Genode;


/*****************
 ** ROM service **
 *****************/

namespace Iso {

	typedef Avl_string<PATH_LENGTH> File_base;
	typedef Avl_tree<Avl_string_base> File_cache;

	class File;
	class Rom_component;

	typedef Genode::Root_component<Rom_component> Root_component;


	class Root;
}


/**
 * File abstraction
 */
class Iso::File : public File_base
{
	private:

		Genode::Allocator        &_alloc;

		File_info                *_info;
		Attached_ram_dataspace    _ds;

	public:

		File(Genode::Env &env, Genode::Allocator &alloc,
		     Block::Connection &block, char const *path)
		:
			File_base(path), _alloc(alloc),
			_info(Iso::file_info(_alloc, block, path)),
			_ds(env.ram(), env.rm(), align_addr(_info->page_sized(), 12))
		{
			Iso::read_file(block, _info, 0, _ds.size(), _ds.local_addr<void>());
		}
		
		~File() { destroy(_alloc, _info); }

		Dataspace_capability dataspace() { return _ds.cap(); }
};


class Iso::Rom_component : public Genode::Rpc_object<Rom_session>
{
	private:

		File *_file;

		File *_lookup(File_cache &cache, char const *path)
		{
			return static_cast<File *>(cache.first() ?
			                           cache.first()->find_by_name(path) :
			                           0);
		}

	public:

		Rom_dataspace_capability dataspace() {
			return static_cap_cast<Rom_dataspace>(_file->dataspace()); }

		void sigh(Signal_context_capability) { }

		Rom_component(Genode::Env &env, Genode::Allocator &alloc,
		              File_cache &cache, Block::Connection &block,
		              char const *path)
		{
			if ((_file = _lookup(cache, path))) {
				Genode::log("cache hit for file ", Genode::Cstring(path));
				return;
			}

			_file = new (alloc) File(env, alloc, block, path);
			Genode::log("request for file ", Genode::Cstring(path));

			cache.insert(_file);
		}
};


class Iso::Root : public Iso::Root_component
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Allocator_avl     _block_alloc { &_alloc };
		Block::Connection _block       { _env, &_block_alloc };

		/*
		 * Entries in the cache are never freed, even if the ROM session
		 * gets destroyed.
		 */
		File_cache _cache;

		char _path[PATH_LENGTH];

	protected:

		Rom_component *_create_session(const char *args)
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			size_t session_size =  sizeof(Rom_component) + sizeof(File_info);
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			Session_label const label = label_from_args(args);
			strncpy(_path, label.last_element().string(), sizeof(_path));

			if (verbose)
				Genode::log("Request for file ", Cstring(_path), " len ", strlen(_path));

			try {
				return new (_alloc) Rom_component(_env, _alloc, _cache, _block, _path);
			}
			catch (Io_error)       { throw Root::Unavailable(); }
			catch (Non_data_disc)  { throw Root::Unavailable(); }
			catch (File_not_found) { throw Root::Invalid_args(); }
		}

	public:

		Root(Genode::Env &env, Allocator &alloc)
		:
			Root_component(&env.ep().rpc_ep(), &alloc),
			_env(env), _alloc(alloc)
		{ }
};


struct Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Iso::Root     _root { _env, _heap };

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
