/*
 * \brief  null filesystem
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__NULL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__NULL_FILE_SYSTEM_H_

#include <vfs/single_file_system.h>

namespace Vfs { class Null_file_system; }


struct Vfs::Null_file_system : Single_file_system
{
	Null_file_system(Xml_node config)
	:
		Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
	{ }

	static char const *name() { return "null"; }


	/********************************
	 ** File I/O service interface **
	 ********************************/

	Write_result write(Vfs_handle *handle, char const *, size_t count, size_t &out_count) override
	{
		out_count = count;

		return WRITE_OK;
	}

	Read_result read(Vfs_handle *vfs_handle, char *, size_t, size_t &out_count) override
	{
		out_count = 0;

		return READ_OK;
	}

	Ftruncate_result ftruncate(Vfs_handle *vfs_handle, size_t) override
	{
		return FTRUNCATE_OK;
	}
};

#endif /* _INCLUDE__VFS__NULL_FILE_SYSTEM_H_ */
