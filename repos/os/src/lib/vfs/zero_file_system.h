/*
 * \brief  zero filesystem
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

#ifndef _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_
#define _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Zero_file_system; }


struct Vfs::Zero_file_system : Single_file_system
{
	Zero_file_system(Vfs::Env&, Genode::Xml_node config)
	:
		Single_file_system(Node_type::CONTINUOUS_FILE, name(),
		                   Node_rwx::rw(), config)
	{ }

	static char const *name()   { return "zero"; }
	char const *type() override { return "zero"; }

	struct Zero_vfs_handle : Single_vfs_handle
	{
		Zero_vfs_handle(Directory_service &ds,
		                File_io_service   &fs,
		                Genode::Allocator &alloc)
		: Single_vfs_handle(ds, fs, alloc, 0) { }

		Read_result read(char *dst, file_size count,
		                 file_size &out_count) override
		{
			memset(dst, 0, count);
			out_count = count;

			return READ_OK;
		}

		Write_result write(char const *, file_size count,
		                   file_size &out_count) override
		{
			out_count = count;

			return WRITE_OK;
		}

		bool read_ready() override { return true; }
	};

	/*********************************
	 ** Directory service interface **
	 *********************************/

	Open_result open(char const  *path, unsigned,
	                 Vfs_handle **out_handle,
	                 Allocator   &alloc) override
	{
		if (!_single_file(path))
			return OPEN_ERR_UNACCESSIBLE;

		try {
			*out_handle = new (alloc) Zero_vfs_handle(*this, *this, alloc);
			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
	}

};

#endif /* _INCLUDE__VFS__ZERO_FILE_SYSTEM_H_ */
