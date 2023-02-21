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

#include <os/buffered_xml.h>
#include <vfs/file_system.h>

namespace Vfs { class Inline_file_system; }


class Vfs::Inline_file_system : public Single_file_system
{
	private:

		Genode::Buffered_xml const _node;

		class Handle : public Single_vfs_handle
		{
			private:

				Inline_file_system const &_fs;

			public:

				Handle(Directory_service        &ds,
				       File_io_service          &fs,
				       Allocator                &alloc,
				       Inline_file_system const &inline_fs)
				:
					Single_vfs_handle(ds, fs, alloc, 0), _fs(inline_fs)
				{ }

				inline Read_result read(Byte_range_ptr const &, size_t &) override;

				Write_result write(Const_byte_range_ptr const &,
				                   size_t &out_count) override
				{
					out_count = 0;
					return WRITE_ERR_INVALID;
				}

				bool read_ready()  const override { return true; }
				bool write_ready() const override { return false; }
		};

	public:

		/**
		 * Constructor
		 *
		 * The 'config' XML node (that points to its content) is stored within
		 * the object after construction time. The underlying backing store
		 * must be kept in tact during the lifefile of the object.
		 */
		Inline_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, name(),
			                   Node_rwx::rx(), config),
			_node(env.alloc(), config)
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
				*out_handle = new (alloc) Handle(*this, *this, alloc, *this);
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }

			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);

			_node.xml().with_raw_content([&] (char const *, size_t size) {
				out.size = size; });

			return result;
		}
};


Vfs::File_io_service::Read_result
Vfs::Inline_file_system::Handle::read(Byte_range_ptr const &dst, size_t &out_count)
{
	_fs._node.xml().with_raw_content([&] (char const *start, size_t const len) {

		/* file read limit is the size of the XML-node content */
		size_t const max_size = len;

		/* current read offset */
		size_t const read_offset = size_t(seek());

		/* maximum read offset, clamped to dataspace size */
		size_t const end_offset = min(dst.num_bytes + read_offset, max_size);

		/* source address within the XML content */
		char const * const src = start + read_offset;

		/* check if end of file is reached */
		if (read_offset >= end_offset) {
			out_count = 0;
			return;
		}

		/* copy-out bytes from ROM dataspace */
		size_t const num_bytes = end_offset - read_offset;

		memcpy(dst.start, src, num_bytes);
		out_count = num_bytes;
	});

	return READ_OK;
}

#endif /* _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_ */
