/*
 * \brief  Interface for creating file-system instances
 * \author Norman Feske
 * \date   2014-04-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_
#define _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_

#include <vfs/file_system.h>

namespace Vfs { class File_system_factory; }


struct Vfs::File_system_factory
{
	virtual File_system *create(Xml_node node) = 0;
};

#endif /* _INCLUDE__VFS__FILE_SYSTEM_FACTORY_H_ */
