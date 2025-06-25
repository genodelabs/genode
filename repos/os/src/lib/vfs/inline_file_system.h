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

		struct Buffered_data
		{
			using Allocated = Genode::Memory::Constrained_allocator::Result;

			Allocated allocated;

			size_t num_bytes = 0;  /* unquoted data size */

			static size_t unquoted_content(Byte_range_ptr const &dst, auto const &node)
			{
				struct Output : Genode::Output, Genode::Noncopyable
				{
					struct {
						char  *dst;
						size_t capacity; /* remaining capacity in bytes */
					};

					void out_char(char c) override
					{
						if (capacity) { *dst++ = c; capacity--; }
					}

					void out_string(char const *s, size_t n) override
					{
						while (n-- && capacity) out_char(*s++);
					}

					Output(char *dst, size_t n) : dst(dst), capacity(n) { }

				} output { dst.start, dst.num_bytes };

				bool quoted = false;
				node.for_each_quoted_line([&] (auto const &line) {
					quoted = true;
					line.print(output);
					if (!line.last) output.out_char('\n');
				});
				if (quoted) {
					if (output.capacity)
						return dst.num_bytes - output.capacity;

					Genode::warning("unquoted content exceeded buffer: ", node);
					return 0ul;
				}

				if (node.num_sub_nodes() != 1) {
					warning("exactly one sub node expected: ", node);
					return 0ul;
				}

				return node.with_sub_node(0u, [&] (auto const &content) {
					return Genode::Xml_generator::generate(dst, content.type(),
						[&] (Genode::Xml_generator &xml) {
							xml.node_attributes(content);
							if (!xml.append_node_content(content, { 20 }))
								warning("inline fs too deeply nested: ", content);
						}).template convert<size_t>(
							[&] (size_t n) { return n; },
							[&] (Genode::Buffer_error) {
								warning("failed to copy node content: ", node);
								return 0ul;
							});
				}, [&] () -> size_t { /* checked above */ return 0ul; });
			}

			static size_t _copy_from_node(Allocated &allocated, Node const &node)
			{
				return allocated.convert<size_t>([&] (Genode::Memory::Allocation &a) {
					return unquoted_content({ (char *)a.ptr, a.num_bytes }, node);
				},
				[&] (Genode::Alloc_error) {
					Genode::warning("inline VFS allocation failed");
					return 0ul;
				});
			}

			void with_bytes(auto const &fn) const
			{
				if (num_bytes)
					allocated.with_result([&] (Genode::Memory::Allocation const &a) {
						fn((char const *)a.ptr, num_bytes); }, [&] (auto) { });
			}

			Buffered_data(Genode::Memory::Constrained_allocator &alloc,
			              Node const &node)
			:
				/* use node size as upper approximation of data size */
				allocated(alloc.try_alloc(node.num_bytes())),
				num_bytes(_copy_from_node(allocated, node))
			{ }
		};

		Buffered_data const _data;

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
		 * The 'config' node (that points to its content) is stored within
		 * the object after construction time. The underlying backing store
		 * must be kept in tact during the lifefile of the object.
		 */
		Inline_file_system(Vfs::Env &env, Node const &config)
		:
			Single_file_system(Node_type::CONTINUOUS_FILE, name(),
			                   Node_rwx::rx(), config),
			_data(env.alloc(), config)
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

			out.size = _data.num_bytes;

			return result;
		}
};


Vfs::File_io_service::Read_result
Vfs::Inline_file_system::Handle::read(Byte_range_ptr const &dst, size_t &out_count)
{
	out_count = 0;

	_fs._data.with_bytes([&] (char const *start, size_t const len) {

		/* file read limit is the size of the node content */
		size_t const max_size = len;

		/* current read offset */
		size_t const read_offset = size_t(seek());

		/* maximum read offset, clamped to dataspace size */
		size_t const end_offset = min(dst.num_bytes + read_offset, max_size);

		/* source address within the content */
		char const * const src = start + read_offset;

		/* check if end of file is reached */
		if (read_offset >= end_offset)
			return;

		/* copy-out bytes from ROM dataspace */
		size_t const num_bytes = end_offset - read_offset;

		memcpy(dst.start, src, num_bytes);
		out_count = num_bytes;
	});

	return READ_OK;
}

#endif /* _INCLUDE__VFS__INLINE_FILE_SYSTEM_H_ */
