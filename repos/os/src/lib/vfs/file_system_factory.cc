/*
 * \brief   File-system factory implementation
 * \author  Norman Feske
 * \date    2014-04-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <base/shared_object.h>

/* supported builtin file systems */
#include <vfs/tar_file_system.h>
#include <vfs/fs_file_system.h>
#include <vfs/terminal_file_system.h>
#include <vfs/null_file_system.h>
#include <vfs/zero_file_system.h>
#include <vfs/block_file_system.h>
#include <vfs/log_file_system.h>
#include <vfs/rom_file_system.h>
#include <vfs/inline_file_system.h>
#include <vfs/rtc_file_system.h>
#include <vfs/ram_file_system.h>
#include <vfs/symlink_file_system.h>


class Default_file_system_factory : public Vfs::Global_file_system_factory
{
	private:

		typedef Genode::String<128> Fs_type_name;
		typedef Genode::String<128> Node_name;
		typedef Genode::String<128> Library_name;

		struct Entry_base : Vfs::File_system_factory,
		                    Genode::List<Entry_base>::Element
		{
			Fs_type_name name;

			Entry_base(Fs_type_name const &name) : name(name) { }

			bool matches(Genode::Xml_node node) const {
				return node.has_type(name.string()); }
		};


		template <typename FILE_SYSTEM>
		struct Builtin_entry : Entry_base
		{
			Builtin_entry() : Entry_base(FILE_SYSTEM::name()) { }

			Vfs::File_system *create(Genode::Xml_node node) override
			{
				return new (Genode::env()->heap()) FILE_SYSTEM(node);
			}
		};

		struct External_entry : Entry_base
		{
			File_system_factory &_fs_factory;

			External_entry(Fs_type_name       const &name,
			               Vfs::File_system_factory &fs_factory)
			:
				Entry_base(name), _fs_factory(fs_factory) { }

			Vfs::File_system *create(Genode::Xml_node node) override {
				return _fs_factory.create(node); }
		};

		Genode::List<Entry_base> _list;

		template <typename FILE_SYSTEM>
		void _add_builtin_fs()
		{
			_list.insert(new (Genode::env()->heap()) Builtin_entry<FILE_SYSTEM>());
		}

		Vfs::File_system *_try_create(Genode::Xml_node node)
		{
			for (Entry_base *e = _list.first(); e; e = e->next())
				if (e->matches(node))
					return e->create(node);

			return 0;
		}

		/**
		 * Return name of factory provided by the shared library
		 */
		static char const *_factory_symbol()
		{
			return "vfs_file_system_factory";
		}

		/**
		 * Return name of VFS node
		 */
		Node_name _node_name(Genode::Xml_node node)
		{
			char node_name [Node_name::capacity()];
			node.type_name(node_name, sizeof(node_name));
			return Node_name(node_name);
		}

		/**
		 * Return matching library name for a given vfs node name
		 */
		Library_name _library_name(Node_name const &node_name)
		{
			char lib_name [Library_name::capacity()];
			Genode::snprintf(lib_name, sizeof(lib_name), "vfs_%s.lib.so",
			                 node_name.string());

			return Library_name(lib_name);
		}

		class Factory_not_available { };

		/**
		 * \throw Factory_not_available
		 */
		Vfs::File_system_factory &_load_factory(Library_name const &lib_name)
		{
			Genode::Shared_object *shared_object = nullptr;

			try {
				shared_object = new (Genode::env()->heap())
					Genode::Shared_object(lib_name.string());

				typedef Vfs::File_system_factory *(*Query_fn)();

				Query_fn query_fn = shared_object->lookup<Query_fn>(_factory_symbol());

				return *query_fn();

			} catch (Genode::Shared_object::Invalid_file) {
				PWRN("could not open '%s'", lib_name.string());
				throw Factory_not_available();

			} catch (Genode::Shared_object::Invalid_symbol) {
				PWRN("could not find symbol '%s' in '%s'",
				     _factory_symbol(), lib_name.string());

				Genode::destroy(Genode::env()->heap(), shared_object);
				throw Factory_not_available();
			}
		}

		bool _probe_external_factory(Genode::Xml_node node)
		{
			Library_name const lib_name = _library_name(_node_name(node));

			try {
				_list.insert(new (Genode::env()->heap())
					External_entry(_node_name(node).string(), _load_factory(lib_name)));
				return true;

			} catch (Factory_not_available) { return false; }
		}

	public:

		Vfs::File_system *create(Genode::Xml_node node) override
		{
			try {
				/* try if type is handled by the currently registered fs types */
				if (Vfs::File_system *fs = _try_create(node))
					return fs;
				/* if the builtin fails, do not try loading an external */
			} catch (...) { return 0; }

			try {
				/* probe for file system implementation available as shared lib */
				if (_probe_external_factory(node)) {
					/* try again with the new file system type loaded */
					if (Vfs::File_system *fs = _try_create(node))
						return fs;
				}
			} catch (...) { }

			return 0;
		}

		void extend(char const *name, File_system_factory &factory) override
		{
			_list.insert(new (Genode::env()->heap())
				External_entry(name, factory));
		}

		Default_file_system_factory()
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
};


namespace Vfs {

	Global_file_system_factory &global_file_system_factory()
	{
		static Default_file_system_factory inst;
		return inst;
	}
}
