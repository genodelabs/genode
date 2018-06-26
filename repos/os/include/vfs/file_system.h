/*
 * \brief  VFS file-system back-end interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__FILE_SYSTEM_H_
#define _INCLUDE__VFS__FILE_SYSTEM_H_

#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <util/xml_node.h>

namespace Vfs { class File_system; }


class Vfs::File_system : public Directory_service, public File_io_service
{
	private:

		/*
		 * Noncopyable
		 */
		File_system(File_system const &);
		File_system &operator = (File_system const &);

	public:

		/**
		 * Our next sibling within the same 'Dir_file_system'
		 */
		struct File_system *next;

		File_system() : next(0) { }

		/**
		 * Adjust to configuration changes
		 */
		virtual void apply_config(Genode::Xml_node const &) { }

		/**
		 * Return the file-system type
		 */
		virtual char const *type() = 0;
};

#endif /* _INCLUDE__VFS__FILE_SYSTEM_H_ */
