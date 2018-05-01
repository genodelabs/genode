/*
 * \brief  GPT utils
 * \author Josef Soentgen
 * \date   2018-05-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/* Genode includes */
#include <base/fixed_stdint.h>
#include <block_session/connection.h>
#include <util/misc_math.h>
#include <util/string.h>


namespace Util {

	using namespace Genode;

	void init_random(Genode::Allocator &);
	void get_random(uint8_t *dest, size_t len);

	using Label = Genode::String<128>;

	using Size_string = Genode::String<64>;
	uint64_t convert(Size_string const &);

	using sector_t = Block::sector_t;

	sector_t align_start(size_t, size_t, sector_t);
	sector_t size_to_lba(size_t, uint64_t);

	struct Block_io;

	uint32_t crc32(void const * const, size_t);
	size_t extract_ascii(char *, size_t, uint16_t const *, size_t);
	size_t convert_ascii(uint16_t *, size_t, uint8_t const *, size_t);

	/*
	 * Wrapper to get suffixed uint64_t values
	 */
	class Number_of_bytes
	{
		uint64_t _n;

		public:

		/**
		 * Default constructor
		 */
		Number_of_bytes() : _n(0) { }

		/**
		 * Constructor, to be used implicitly via assignment operator
		 */
		Number_of_bytes(Genode::uint64_t n) : _n(n) { }

		/**
		 * Convert number of bytes to 'size_t' value
		 */
		operator Genode::uint64_t() const { return _n; }

		void print(Output &output) const
		{
			using Genode::print;

			enum { KB = 1024UL, MB = KB*1024UL, GB = MB*1024UL };

			if      (_n      == 0) print(output, 0);
			else if (_n % GB == 0) print(output, _n/GB, "G");
			else if (_n % MB == 0) print(output, _n/MB, "M");
			else if (_n % KB == 0) print(output, _n/KB, "K");
			else                   print(output, _n);
		}
	};

	inline size_t ascii_to(const char *s, Number_of_bytes &result)
	{
		unsigned long res = 0;

		/* convert numeric part of string */
		int i = ascii_to_unsigned(s, res, 0);

		/* handle suffixes */
		if (i > 0)
			switch (s[i]) {
				case 'G': res *= 1024;
				case 'M': res *= 1024;
				case 'K': res *= 1024; i++;
				default: break;
			}

		result = res;
		return i;
	}
};


/*
 * Block_io wraps a Block::Connection for synchronous operations
 */
struct Util::Block_io
{
	struct Io_error : Genode::Exception { };

	using Packet_descriptor = Block::Packet_descriptor;

	Block::Connection  &_block;
	Packet_descriptor  _p;

	/**
	 * Constructor
	 *
	 * \param block  reference to underlying Block::Connection
	 * \param block_size  logical block size of the Block::Connection
	 * \param lba         LBA to start access from
	 * \param count       number of LBAs to access
	 * \param write       set type of operation, write if true, read
	 *                    if false
	 *
	 * \throw Io_error
	 */
	Block_io(Block::Connection &block, size_t block_size,
	         sector_t lba, size_t count,
	         bool write = false, void const *data = nullptr, size_t len = 0)
	:
		_block(block),
		_p(_block.tx()->alloc_packet(block_size * count),
		   write ? Packet_descriptor::WRITE
		         : Packet_descriptor::READ, lba, count)
	{
		if (write) {
			if (data && len) {
				void *p = addr<void*>();
				Genode::memcpy(p, data, len);
			} else {
				Genode::error("invalid data for write");
				throw Io_error();
			}
		}

		_block.tx()->submit_packet(_p);
		_p = _block.tx()->get_acked_packet();
		if (!_p.succeeded()) {
			Genode::error("could not ", write ? "write" : "read",
			              " block-range [", _p.block_number(), ",",
			              _p.block_number() + count, ")");
			_block.tx()->release_packet(_p);
			throw Io_error();
		}
	}

	~Block_io() { _block.tx()->release_packet(_p); }

	template <typename T> T addr()
	{
		return reinterpret_cast<T>(_block.tx()->packet_content(_p));
	}
};

#endif /* _UTIL_H_ */
