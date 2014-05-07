/*
 * \brief  zero filesystem
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

#ifndef _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_
#define _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Zero_file_system; }


struct Vfs::Zero_file_system : Single_file_system
{
	Zero_file_system(Xml_node config)
	:
		Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
	{ }

	static char const *name() { return "zero"; }


	/********************************
	 ** File I/O service interface **
	 ********************************/

	Write_result write(Vfs_handle *, char const *, size_t count, size_t &count_out) override
	{
		count_out = count;

		return WRITE_OK;
	}

	Read_result read(Vfs_handle *vfs_handle, char *dst, size_t count, size_t &out_count) override
	{
		memset(dst, 0, count);
		out_count = count;

		return READ_OK;
	}
};

#endif /* _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_ */
