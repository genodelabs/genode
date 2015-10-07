/*
 * \brief  VFS file-system back-end interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_H_
#define _INCLUDE__VFS__FILE_SYSTEM_H_

#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>

namespace Vfs { struct File_system; }


struct Vfs::File_system : Directory_service, File_io_service
{
	/**
	 * Our next sibling within the same 'Dir_file_system'
	 */
	struct File_system *next;

	File_system() : next(0) { }

	/**
	 * Synchronize file system
	 *
	 * This method flushes any delayed operations from the file system.
	 */
	virtual void sync(char const *path) { }
};

#endif /* _INCLUDE__VFS__FILE_SYSTEM_H_ */
