/*
 * \brief   File-system factory implementation
 * \author  Norman Feske
 * \date    2014-04-09
 */

/*
 * Copyright (C) 2014-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <vfs/file_system_factory.h>
#include <base/shared_object.h>

/* supported builtin file systems */
#include <block_file_system.h>
#include <fs_file_system.h>
#include <inline_file_system.h>
#include <log_file_system.h>
#include <null_file_system.h>
#include <ram_file_system.h>
#include <rom_file_system.h>
#include <rtc_file_system.h>
#include <symlink_file_system.h>
#include <tar_file_system.h>
#include <terminal_file_system.h>
#include <zero_file_system.h>


namespace Vfs {

	using Vfs::Io_response_handler;

	template <typename> struct Builtin_entry;
	struct External_entry;
}


using Fs_type_name = Vfs::Global_file_system_factory::Fs_type_name;
using Node_name    = Vfs::Global_file_system_factory::Node_name;
using Library_name = Vfs::Global_file_system_factory::Library_name;


struct Vfs::Global_file_system_factory::Entry_base : Vfs::File_system_factory,
                                                     private Genode::List<Entry_base>::Element
{
	friend class Genode::List<Entry_base>;

	using Genode::List<Entry_base>::Element::next;

	Fs_type_name name;

	Entry_base(Fs_type_name const &name) : name(name) { }

	bool matches(Genode::Xml_node node) const {
		return node.has_type(name.string()); }
};


template <typename FILE_SYSTEM>
struct Vfs::Builtin_entry : Vfs::Global_file_system_factory::Entry_base
{
	Builtin_entry() : Entry_base(FILE_SYSTEM::name()) { }

	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override {
		return new (env.alloc()) FILE_SYSTEM(env, node); }
};


struct Vfs::External_entry : Vfs::Global_file_system_factory::Entry_base
{
	File_system_factory &_fs_factory;

	External_entry(Fs_type_name  const &name,
	               File_system_factory &fs_factory)
	:
		Entry_base(name), _fs_factory(fs_factory) { }

	File_system *create(Vfs::Env &env,
	                    Genode::Xml_node config) override
	{
		return _fs_factory.create(env, config);
	}
};


/**
 * Add builtin File_system type
 */
template <typename FILE_SYSTEM>
void Vfs::Global_file_system_factory::_add_builtin_fs()
{
	_list.insert(new (&_md_alloc) Builtin_entry<FILE_SYSTEM>());
}


/**
 * Lookup and create File_system instance
 */
Vfs::File_system*
Vfs::Global_file_system_factory::_try_create(Vfs::Env &env,
		                                     Genode::Xml_node config)
{
	for (Entry_base *e = _list.first(); e; e = e->next())
		if (e->matches(config)) return e->create(env, config);
	return nullptr;
}


/**
 * Return name of VFS node
 */
Node_name Vfs::Global_file_system_factory::_node_name(Genode::Xml_node node)
{
	char node_name [Node_name::capacity()];
	node.type_name(node_name, sizeof(node_name));
	return Node_name(node_name);
}


/**
 * Return matching library name for a given vfs node name
 */
Library_name Vfs::Global_file_system_factory::_library_name(Node_name const &node_name)
{
	char lib_name [Library_name::capacity()];
	Genode::snprintf(lib_name, sizeof(lib_name), "vfs_%s.lib.so",
	                 node_name.string());

	return Library_name(lib_name);
}


/**
 * \throw Factory_not_available
 */
Vfs::File_system_factory &Vfs::Global_file_system_factory::_load_factory(Vfs::Env &env,
                                                                         Library_name const &lib_name)
{
	Genode::Shared_object *shared_object = nullptr;

	try {
		shared_object = new (env.alloc())
			Genode::Shared_object(env.env(), env.alloc(), lib_name.string(),
			                      Genode::Shared_object::BIND_LAZY,
			                      Genode::Shared_object::DONT_KEEP);

		typedef Vfs::File_system_factory *(*Query_fn)();

		Query_fn query_fn = shared_object->lookup<Query_fn>(_factory_symbol());

		return *query_fn();

	} catch (Genode::Shared_object::Invalid_rom_module) {
		Genode::warning("could not open '", lib_name, "'");
		throw Factory_not_available();

	} catch (Genode::Shared_object::Invalid_symbol) {
		Genode::warning("could not find symbol '",
		                Genode::Cstring(_factory_symbol()),
		                "' in '", lib_name, "'");

		Genode::destroy(env.alloc(), shared_object);
		throw Factory_not_available();
	}
}


/**
 * Try to load external File_system_factory provider
 */
bool Vfs::Global_file_system_factory::_probe_external_factory(Vfs::Env       &env,
                                                              Genode::Xml_node   node)
{
	Library_name const lib_name = _library_name(_node_name(node));

	try {
		_list.insert(new (env.alloc())
			External_entry(_node_name(node).string(),
			               _load_factory(env, lib_name)));
		return true;

	} catch (Factory_not_available) { return false; }
}


/**
 * Create and return a new file-system
 */
Vfs::File_system *Vfs::Global_file_system_factory::create(Vfs::Env         &env,
                                                          Genode::Xml_node  node)
{
	try {
		/* try if type is handled by the currently registered fs types */
		if (Vfs::File_system *fs = _try_create(env, node))
			return fs;
		/* if the builtin fails, do not try loading an external */
	} catch (...) { return nullptr; }

	try {
		/* probe for file system implementation available as shared lib */
		if (_probe_external_factory(env, node)) {
			/* try again with the new file system type loaded */
			if (Vfs::File_system *fs = _try_create(env, node))
				return fs;
		}
	} catch (...) { }

	return nullptr;
}


/**
 * Register an additional factory for new file-system type
 */
void Vfs::Global_file_system_factory::extend(char const *name, File_system_factory &factory)
{
	_list.insert(new (&_md_alloc)
		External_entry(name, factory));
}


/**
 * Constructor
 */
Vfs::Global_file_system_factory::Global_file_system_factory(Genode::Allocator &alloc)
:
	_md_alloc(alloc)
{
	_add_builtin_fs<Vfs::Tar_file_system>();
	_add_builtin_fs<Vfs::Fs_file_system>();
	_add_builtin_fs<Vfs::Terminal_file_system>();
	_add_builtin_fs<Vfs::Null_file_system>();
	_add_builtin_fs<Vfs::Zero_file_system>();
	_add_builtin_fs<Vfs::Block_file_system>();
	_add_builtin_fs<Vfs::Log_file_system>();
	_add_builtin_fs<Vfs::Rom_file_system>();
	_add_builtin_fs<Vfs::Inline_file_system>();
	_add_builtin_fs<Vfs::Rtc_file_system>();
	_add_builtin_fs<Vfs::Ram_file_system>();
	_add_builtin_fs<Vfs::Symlink_file_system>();
}
