/*
 * \brief  null filesystem
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__NULL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__NULL_FILE_SYSTEM_H_

#include <vfs/single_file_system.h>

namespace Vfs { class Null_file_system; }


struct Vfs::Null_file_system : Single_file_system
{
	Null_file_system(Genode::Env&,
	                 Genode::Allocator&,
	                 Genode::Xml_node config,
	                 Io_response_handler &)
	:
		Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
	{ }

	static char const *name()   { return "null"; }
	char const *type() override { return "null"; }

	/********************************
	 ** File I/O service interface **
	 ********************************/

	Write_result write(Vfs_handle *handle, char const *, file_size count,
	                   file_size &out_count) override
	{
		out_count = count;

		return WRITE_OK;
	}

	Read_result read(Vfs_handle *vfs_handle, char *, file_size,
	                 file_size &out_count) override
	{
		out_count = 0;

		return READ_OK;
	}

	bool read_ready(Vfs_handle *) override { return false; }

	Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
	{
		return FTRUNCATE_OK;
	}
};

#endif /* _INCLUDE__VFS__NULL_FILE_SYSTEM_H_ */
