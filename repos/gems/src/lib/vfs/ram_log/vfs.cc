/*
 * \brief  RAM-capped log file
 * \author Norman Feske
 * \date   2025-10-22
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/vfs.h>
#include <vfs/single_file_system.h>

using namespace Genode;


namespace Vfs { struct Ram_log_file_system; }


struct Vfs::Ram_log_file_system : Single_file_system
{
	Allocator &_alloc;

	struct Buffer
	{
		Allocator &_alloc;
		size_t const _limit;

		Byte_range_ptr _bytes { (char *)_alloc.alloc(_limit), _limit };

		uint64_t _write_pos = 0;

		Buffer(Allocator &alloc, size_t limit) : _alloc(alloc), _limit(max(limit, 1ul)) { }

		~Buffer() { _alloc.free(_bytes.start, _bytes.num_bytes); }

		void append(char c)
		{
			_bytes.start[_write_pos % _limit] = c;
			_write_pos++;
		}

		Attempt<char, Buffer_error> byte_at(uint64_t pos) const
		{
			if (pos >= _write_pos)
				return Buffer_error::EXCEEDED;

			uint64_t const distance_from_end = _write_pos - pos;

			if (distance_from_end > _limit)
				return 0; /* return zeros for evicted content */

			return _bytes.start[pos % _limit];
		}

	} _buffer;

	struct Handle : Single_vfs_handle
	{
		Ram_log_file_system &_ram_log;

		Handle(Directory_service &ds, File_io_service &fs, Allocator &alloc,
		       Ram_log_file_system &ram_log)
		:
			Single_vfs_handle { ds, fs, alloc, 0 }, _ram_log(ram_log)
		{ }

		Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
		{
			out_count = 0;
			for (size_t i = 0; i < dst.num_bytes; i++) {

				auto const byte = _ram_log._buffer.byte_at(seek());
				if (byte.failed())
					break;

				byte.with_result([&] (char c) {
					dst.start[i] = c;
					advance_seek(1);
					out_count++;
				}, [&] (auto) { });
			}
			return READ_OK;
		}

		Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
		{
			if (seek() != _ram_log._buffer._write_pos) {
				warning("vfs_ram_log is append-only, reset write position to ", seek());
				_ram_log._buffer._write_pos = seek();
			}

			for (size_t i = 0; i < src.num_bytes; i++)
				_ram_log._buffer.append(src.start[i]);

			advance_seek(src.num_bytes);
			out_count = src.num_bytes;

			return WRITE_OK;
		}

		bool read_ready()  const override { return true; }
		bool write_ready() const override { return true; }
	};

	Ram_log_file_system(Vfs::Env &vfs_env, Node const &config)
	:
		Single_file_system(Node_type::CONTINUOUS_FILE, name(), Node_rwx::ro(), config),
		_alloc(vfs_env.alloc()),
		_buffer(_alloc, config.attribute_value("limit", Num_bytes { 16*1024 }))
	{ }

	static char const *name()   { return "ram_log"; }
	char const *type() override { return "ram_log"; }

	Open_result open(char const *path, unsigned, Vfs_handle **out_handle,
	                 Allocator &alloc) override
	{
		if (!_single_file(path))
			return OPEN_ERR_UNACCESSIBLE;

		try {
			*out_handle =
				new (alloc) Handle(*this, *this, alloc, *this);
			return OPEN_OK;
		}
		catch (Genode::Out_of_ram)        { return OPEN_ERR_OUT_OF_RAM; }
		catch (Genode::Out_of_caps)       { return OPEN_ERR_OUT_OF_CAPS; }
		/* handled non-existing path */
		catch (Genode::File::Open_failed) { return OPEN_ERR_UNACCESSIBLE; }
	}

	Stat_result stat(char const *path, Stat &out) override
	{
		Stat_result result = Single_file_system::stat(path, out);
		out.size = _buffer._write_pos;
		return result;
	}
};


struct Ram_log_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Vfs::Env &env, Node const &node) override
	{
		return new (env.alloc()) Vfs::Ram_log_file_system(env, node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Ram_log_factory factory;
	return &factory;
}
