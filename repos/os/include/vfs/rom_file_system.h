/*
 * \brief  ROM filesystem
 * \author Norman Feske
 * \date   2014-04-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__ROM_FILE_SYSTEM_H_
#define _INCLUDE__VFS__ROM_FILE_SYSTEM_H_

#include <os/attached_rom_dataspace.h>
#include <vfs/file_system.h>

namespace Vfs { class Rom_file_system; }


class Vfs::Rom_file_system : public Single_file_system
{
	private:

		struct Label
		{
			enum { LABEL_MAX_LEN = 64 };
			char string[LABEL_MAX_LEN];

			Label(Xml_node config)
			{
				/* obtain label from config */
				string[0] = 0;
				try { config.attribute("label").value(string, sizeof(string)); }
				catch (...)
				{
					/* use VFS node name if label was not provided */
					string[0] = 0;
					try { config.attribute("name").value(string, sizeof(string)); }
					catch (...) { }
				}
			}
		} _label;

		Genode::Attached_rom_dataspace _rom;

	public:

		Rom_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_FILE, name(), config),
			_label(config),
			_rom(_label.string)
		{ }

		static char const *name() { return "rom"; }


		/*********************************
		 ** Directory-service interface **
		 ********************************/

		/*
		 * Overwrite the default open function to update the ROM dataspace
		 * each time when opening the corresponding file.
		 */
		Open_result open(char const *path, unsigned,
		                 Vfs_handle **out_handle) override
		{
			Open_result const result =
				Single_file_system::open(path, 0, out_handle);

			_rom.update();

			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);

			_rom.update();
			out.size = _rom.is_valid() ? _rom.size() : 0;

			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *, file_size,
		                   file_size &count_out) override
		{
			count_out = 0;
			return WRITE_ERR_INVALID;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			/* file read limit is the size of the dataspace */
			file_size const max_size = _rom.size();

			/* current read offset */
			file_size const read_offset = vfs_handle->seek();

			/* maximum read offset, clamped to dataspace size */
			file_size const end_offset = min(count + read_offset, max_size);

			/* source address within the dataspace */
			char const *src = _rom.local_addr<char>() + read_offset;

			/* check if end of file is reached */
			if (read_offset >= end_offset) {
				out_count = 0;
				return READ_OK;
			}

			/* copy-out bytes from ROM dataspace */
			file_size const num_bytes = end_offset - read_offset;

			memcpy(dst, src, num_bytes);

			out_count = num_bytes;
			return READ_OK;
		}
};

#endif /* _INCLUDE__VFS__ROM_FILE_SYSTEM_H_ */
