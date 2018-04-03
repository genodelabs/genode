/*
 * \brief  Inline filesystem
 * \author Norman Feske
 * \date   2014-04-14
 *
 * This file system allows the content of a file being specified as the content
 * of its config node.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_
#define _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_

#include <vfs/file_system.h>

namespace Vfs { class Inline_file_system; }


class Vfs::Inline_file_system : public Single_file_system
{
	private:

		char const * const _base;
		file_size    const _size;

		/*
		 * Noncopyable
		 */
		Inline_file_system(Inline_file_system const &);
		Inline_file_system &operator = (Inline_file_system const &);

		class Inline_vfs_handle : public Single_vfs_handle
		{
			private:

				char const * const _base;
				file_size    const _size;

				/*
				 * Noncopyable
				 */
				Inline_vfs_handle(Inline_vfs_handle const &);
				Inline_vfs_handle &operator = (Inline_vfs_handle const &);

			public:

				Inline_vfs_handle(Directory_service &ds,
				               File_io_service      &fs,
				               Genode::Allocator    &alloc,
				               char const * const    base,
				               file_size    const    size)
				: Single_vfs_handle(ds, fs, alloc, 0),
				  _base(base), _size(size)
				{ }

				Read_result read(char *dst, file_size count,
				                 file_size &out_count) override
				{
					/* file read limit is the size of the dataspace */
					file_size const max_size = _size;

					/* current read offset */
					file_size const read_offset = seek();

					/* maximum read offset, clamped to dataspace size */
					file_size const end_offset = min(count + read_offset, max_size);

					/* source address within the dataspace */
					char const *src = _base + read_offset;

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

				Write_result write(char const *, file_size,
				                   file_size &out_count) override
				{
					out_count = 0;
					return WRITE_ERR_INVALID;
				}

				bool read_ready() { return true; }
		};

	public:

		Inline_file_system(Vfs::Env&, Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_FILE, name(), config),
			_base(config.content_base()),
			_size(config.content_size())
		{ }

		static char const *name()   { return "inline"; }
		char const *type() override { return "inline"; }

		/********************************
		 ** Directory service interface **
		 ********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc)
					Inline_vfs_handle(*this, *this, alloc, _base, _size);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _size;
			return result;
		}
};

#endif /* _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_ */
