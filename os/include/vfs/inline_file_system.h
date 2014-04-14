/*
 * \brief  Inline filesystem
 * \author Norman Feske
 * \date   2014-04-14
 *
 * This file system allows the content of a file being specified as the content
 * of its config node.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_
#define _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Inline_file_system; }


class Vfs::Inline_file_system : public Single_file_system
{
	private:

		char const * const _base;
		size_t       const _size;

	public:

		Inline_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_FILE, name(), config),
			_base(config.content_base()),
			_size(config.content_size())
		{ }

		static char const *name() { return "inline"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _size;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *, size_t, size_t &count_out) override
		{
			count_out = 0;
			return WRITE_ERR_INVALID;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, size_t count,
		                 size_t &out_count) override
		{
			/* file read limit is the size of the dataspace */
			size_t const max_size = _size;

			/* current read offset */
			size_t const read_offset = vfs_handle->seek();

			/* maximum read offset, clamped to dataspace size */
			size_t const end_offset = min(count + read_offset, max_size);

			/* source address within the dataspace */
			char const *src = _base + read_offset;

			/* check if end of file is reached */
			if (read_offset >= end_offset) {
				out_count = 0;
				return READ_OK;
			}

			/* copy-out bytes from ROM dataspace */
			size_t const num_bytes = end_offset - read_offset;

			memcpy(dst, src, num_bytes);

			out_count = num_bytes;
			return READ_OK;
		}
};

#endif /* _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_ */
