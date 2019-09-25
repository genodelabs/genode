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
	Null_file_system(Vfs::Env&, Genode::Xml_node config)
	:
		Single_file_system(Node_type::CONTINUOUS_FILE, name(),
		                   Node_rwx::rw(), config)
	{ }

	static char const *name()   { return "null"; }
	char const *type() override { return "null"; }

	struct Null_vfs_handle : Single_vfs_handle
	{
		Null_vfs_handle(Directory_service &ds,
		                File_io_service   &fs,
		                Genode::Allocator &alloc)
		: Single_vfs_handle(ds, fs, alloc, 0) { }

		Read_result read(char *, file_size, file_size &out_count) override
		{
			out_count = 0;

			return READ_OK;
		}

		Write_result write(char const *, file_size count,
		                   file_size &out_count) override
		{
			out_count = count;

			return WRITE_OK;
		}

		bool read_ready() override { return false; }
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
			*out_handle = new (alloc)
				Null_vfs_handle(*this, *this, alloc);
			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
	}

	/********************************
	 ** File I/O service interface **
	 ********************************/

	Ftruncate_result ftruncate(Vfs_handle *, file_size) override
	{
		return FTRUNCATE_OK;
	}
};

#endif /* _INCLUDE__VFS__NULL_FILE_SYSTEM_H_ */
