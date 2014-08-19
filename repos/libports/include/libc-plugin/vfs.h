/*
 * \brief  vfs plugin interface
 * \author Josef Soentgen
 * \date   2014-08-19
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_PLUGIN__VFS_H_
#define _LIBC_PLUGIN__VFS_H_

/* Genode includes */
#include <vfs/file_system.h>
#include <util/xml_node.h>

namespace Libc {
	struct File_system_factory;

	typedef Libc::File_system_factory*(*File_system_factory_func)(void);
}

struct Libc::File_system_factory
{
	char const * const name;

	File_system_factory(char const *name) : name(name) { }

	virtual Vfs::File_system *create(Genode::Xml_node node) = 0;

	virtual bool matches(Genode::Xml_node node) const {
		return node.has_type(name); }
};

#endif /* _LIBC_PLUGIN__VFS_H_ */
