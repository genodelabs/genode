/*
 * \brief  Interface for creating file-system instances
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_
#define _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_

#include <vfs/file_system.h>

namespace Vfs {

	struct File_system_factory;
	struct Global_file_system_factory;

	/**
	 * Return singleton instance of a file-system factory
	 */
	Global_file_system_factory &global_file_system_factory();
}


struct Vfs::File_system_factory
{
	/**
	 * Create and return a new file-system
	 *
	 * \param env     Env for service connections
	 * \param alloc   internal file-system allocator
	 * \param config  file-system configuration
	 */
	virtual File_system *create(Genode::Env &env,
	                            Genode::Allocator &alloc,
	                            Xml_node config) = 0;
};


struct Vfs::Global_file_system_factory : File_system_factory
{
	/**
	 * Register an additional factory for new file-system type
	 *
	 * \name     name of file-system type
	 * \factory  factory to create instances of this file-system type
	 */
	virtual void extend(char const *name, File_system_factory &factory) = 0;
};


#endif /* _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_ */
