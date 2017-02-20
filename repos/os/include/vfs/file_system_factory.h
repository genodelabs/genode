/*
 * \brief  Interface for creating file-system instances
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_
#define _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_

#include <vfs/file_system.h>

namespace Vfs {

	struct File_system_factory;
	struct Global_file_system_factory;
}


struct Vfs::File_system_factory
{
	/**
	 * Create and return a new file-system
	 *
	 * \param env     Env for service connections
	 * \param alloc   internal file-system allocator
	 * \param config  file-system configuration
	 * \param io_handler  callback handler
	 */
	virtual File_system *create(Genode::Env &env,
	                            Genode::Allocator &alloc,
	                            Xml_node config,
	                            Io_response_handler &io_handler) = 0;
};


class Vfs::Global_file_system_factory : public Vfs::File_system_factory
{
	private:

		Genode::Allocator &_md_alloc;

	public:

		typedef Genode::String<128> Fs_type_name;
		typedef Genode::String<128> Node_name;
		typedef Genode::String<128> Library_name;

		struct Entry_base;

	private:

		Genode::List<Entry_base> _list;

		/**
		 * Add builtin File_system type
		 */
		template <typename FILE_SYSTEM>
		void _add_builtin_fs();

		Vfs::File_system *_try_create(Genode::Env       &env,
		                              Genode::Allocator &alloc,
		                              Genode::Xml_node   node,
		                              Io_response_handler &io_handler);

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
		Node_name _node_name(Genode::Xml_node node);

		/**
		 * Return matching library name for a given vfs node name
		 */
		Library_name _library_name(Node_name const &node_name);

		class Factory_not_available { };

		/**
		 * \throw Factory_not_available
		 */
		Vfs::File_system_factory &_load_factory(Genode::Env        &env,
		                                        Genode::Allocator  &alloc,
		                                        Library_name const &lib_name);

		/**
		 * Try to load external File_system_factory provider
		 */
		bool _probe_external_factory(Genode::Env &env, Genode::Allocator &alloc,
		                             Genode::Xml_node node);

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  internal factory allocator
		 */
		Global_file_system_factory(Genode::Allocator &md_alloc);

		/**
		 * Create and return a new file-system
		 *
		 * \param env         Env for service connections
		 * \param alloc       internal file-system allocator
		 * \param config      file-system configuration
		 * \param io_handler  callback handler
		 */
		Vfs::File_system *create(Genode::Env       &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node   node,
		                         Io_response_handler &io_handler) override;

		/**
		 * Register an additional factory for new file-system type
		 *
		 * \name     name of file-system type
		 * \factory  factory to create instances of this file-system type
		 */
		void extend(char const *name, File_system_factory &factory);
};

#endif /* _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_ */
