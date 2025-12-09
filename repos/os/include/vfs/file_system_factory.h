/*
 * \brief  Interface for creating file-system instances
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_
#define _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_

#include <vfs/env.h>
#include <vfs/file_system.h>

namespace Genode::Vfs {

	struct File_system_factory;
	struct Global_file_system_factory;
}


struct Genode::Vfs::File_system_factory : Interface
{
	/**
	 * Create and return a new file-system
	 *
	 * \param env         Env of VFS root
	 * \param config      file-system configuration
	 */
	virtual File_system *create(Env &env, Node const &config) = 0;
};


class Genode::Vfs::Global_file_system_factory : public File_system_factory
{
	private:

		Allocator &_md_alloc;

	public:

		using Fs_type_name = String<128>;
		using Node_name    = String<128>;
		using Library_name = String<128>;

		struct Entry_base;

	private:

		List<Entry_base> _list { };

		/**
		 * Add builtin File_system type
		 */
		template <typename FILE_SYSTEM>
		void _add_builtin_fs();

		File_system *_try_create(Env &env, Node const &config);

		/**
		 * Return name of factory provided by the shared library
		 */
		static char const *_factory_symbol()
		{
			return "vfs_file_system_factory";
		}

		/**
		 * Return matching library name for a given vfs node name
		 */
		Library_name _library_name(Node_name const &node_name);

		class Factory_not_available { };

		/**
		 * \throw Factory_not_available
		 */
		File_system_factory &_load_factory(Env &env, Library_name const &lib_name);

		/**
		 * Try to load external File_system_factory provider
		 */
		bool _probe_external_factory(Env &env, Node const &node);

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  internal factory allocator
		 */
		Global_file_system_factory(Allocator &alloc);

		/**
		 * File_system_factory interface
		 */
		File_system *create(Env&, Node const &) override;

		/**
		 * Register an additional factory for new file-system type
		 *
		 * \name     name of file-system type
		 * \factory  factory to create instances of this file-system type
		 */
		void extend(char const *name, File_system_factory &factory);
};

#endif /* _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_ */
