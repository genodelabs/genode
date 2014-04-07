/*
 * \brief  File-system related interfaces for Noux
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FILE_SYSTEM_H_
#define _NOUX__FILE_SYSTEM_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <util/xml_node.h>

/* Noux includes */
#include <directory_service.h>
#include <file_io_service.h>
#include <path.h>
#include <noux_session/sysio.h>

namespace Noux {

	struct File_system : Directory_service, File_io_service
	{
		/**
		 * Our next sibling within the same 'Dir_file_system'
		 */
		struct File_system *next;

		File_system() : next(0) { }

		/**
		 * Synchronize file system
		 *
		 * This function is only used by a Fs_file_system because such a
		 * file system may employ a backend, which maintains a internal
		 * cache, that needs to be flushed.
		 */
		virtual void sync() { }
	};
}

#endif /* _NOUX__FILE_SYSTEM_H_ */
