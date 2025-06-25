/*
 * \brief  Block session testing - random test
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_RANDOM_H_
#define _TEST_RANDOM_H_

#include <types.h>

namespace Test { struct Random; }


namespace Util {

	using namespace Genode;

	/*
	 * Xoroshiro128+ written in 2014-2016 by Sebastiano Vigna (vigna@acm.org)
	 *
	 * (see http://xoroshiro.di.unimi.it/xorshift128plus.c and
	 *      http://xoroshiro.di.unimi.it/splitmix64.c)
	 */

	struct Xoroshiro
	{
		uint64_t seed;

		uint64_t splitmix64()
		{
			uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
			z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
			z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
			return z ^ (z >> 31);
		}

		Xoroshiro(uint64_t seed) : seed(seed)
		{
			s[0] = splitmix64();
			s[1] = splitmix64();
		}

		uint64_t s[2];

		static uint64_t rotl(uint64_t const x, int k) {
			return (x << k) | (x >> (64 - k));
		}

		uint64_t get()
		{
			uint64_t const s0 = s[0];
			uint64_t       s1 = s[1];
			uint64_t const result = s0 + s1;

			s1 ^= s0;

			s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
			s[1] = rotl(s1, 36);

			return result;
		}
	};
}


/*
 * Random test
 *
 * This test reads or writes the given number of bytes in
 * sized requests in a deterministic order that depends on
 * the seed value of a PRNG.
 */
struct Test::Random : Scenario
{
	Util::Xoroshiro _random;

	size_t   const _size;
	uint64_t const _length;
	bool     const _r;
	bool     const _w;
	bool     const _alternate_access = _r && _w;

	Block::Operation::Type const _op_type = _w ? Block::Operation::Type::WRITE
	                                           : Block::Operation::Type::READ;

	Block_count    _block_count { };     /* assigned by init() */
	Operation_size _op_size     { };

	block_number_t _next_block()
	{
		uint64_t r = 0;
		block_number_t max = _block_count.blocks;
		if (max >= _op_size.blocks + 1)
			max -= _op_size.blocks + 1;
		do {
			r = _random.get() % max;
		} while (r + _op_size.blocks > _block_count.blocks);

		return r;
	}

	Random(Allocator &, Node const &node)
	:
		Scenario(node),
		_random(node.attribute_value("seed",   42UL)),
		_size  (node.attribute_value("size",   Number_of_bytes())),
		_length(node.attribute_value("length", Number_of_bytes())),
		_r     (node.attribute_value("read",   false)),
		_w     (node.attribute_value("write",  false))
	{ }

	bool init(Init_attr const &attr) override
	{
		if (!_size || !_length) {
			error("request size or length invalid");
			return false;
		}

		if (_size > attr.scratch_buffer_size) {
			error("request size exceeds scratch buffer size");
			return false;
		}

		if (attr.block_size > _size || (_size % attr.block_size) != 0) {
			error("request size invalid ", attr.block_size, " ", _size);
			return false;
		}

		_block_count = { attr.block_count };
		_op_size     = { _size / attr.block_size };
		return true;
	}

	Next_job_result next_job(Stats const &stats) override
	{
		if (stats.total.bytes >= _length)
			return No_job();

		block_number_t const lba = _next_block();

		Block::Operation::Type const op_type =
			_alternate_access ? (lba & 0x1) ? Block::Operation::Type::WRITE
			                                : Block::Operation::Type::READ
			                  : _op_type;

		return Block::Operation { .type         = op_type,
		                          .block_number = lba,
		                          .count        = _op_size.blocks };
	}

	size_t request_size() const override { return _size; }

	char const *name() const override { return "random"; }

	void print(Output &out) const override
	{
		Genode::print(out, name(), " "
		                   "size:",   Number_of_bytes(_size),   " "
		                   "length:", Total(_length), " ");
	}
};

#endif /* _TEST_RANDOM_H_ */
