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
 * This test reads or writes the given number of bytes in a
 * deterministic order that depends and the seed value of a
 * PRNG in particular sized requests.
 */
struct Test::Random : Test_base
{
	bool _alternate_access { false };

	Util::Xoroshiro _random;

	size_t   const _size   = _node.attribute_value("size",   Number_of_bytes());
	uint64_t const _length = _node.attribute_value("length", Number_of_bytes());

	Block::Operation::Type _op_type = Block::Operation::Type::READ;

	block_number_t _next_block()
	{
		uint64_t r = 0;
		block_number_t max = _info.block_count;
		if (max >= _size_in_blocks + 1)
			max -= _size_in_blocks + 1;
		do {
			r = _random.get() % max;
		} while (r + _size_in_blocks > _info.block_count);

		return r;
	}

	template <typename... ARGS>
	Random(ARGS &&...args)
	:
		Test_base(args...),
		_random(_node.attribute_value("seed", 42UL))
	{ }

	void _init() override
	{
		if (_size > _scratch_buffer.size) {
			error("request size exceeds scratch buffer size");
			throw Constructing_test_failed();
		}

		if (!_size || !_length) {
			error("request size or length invalid");
			throw Constructing_test_failed();
		}

		if (_info.block_size > _size || (_size % _info.block_size) != 0) {
			error("request size invalid ", _info.block_size, " ", _size);
			throw Constructing_test_failed();
		}

		bool const r = _node.attribute_value("read", false);
		if (r) { _op_type = Block::Operation::Type::READ; }

		bool const w = _node.attribute_value("write", false);
		if (w) { _op_type = Block::Operation::Type::WRITE; }

		_alternate_access = w && r;

		_size_in_blocks   = _size   / _info.block_size;
		_length_in_blocks = _length / _info.block_size;
	}

	void _spawn_job() override
	{
		if (_bytes >= _length)
			return;

		_job_cnt++;

		block_number_t const lba = _next_block();

		Block::Operation::Type const op_type =
			_alternate_access ? (lba & 0x1) ? Block::Operation::Type::WRITE
			                                : Block::Operation::Type::READ
			                  : _op_type;

		Block::Operation const operation { .type         = op_type,
		                                   .block_number = lba,
		                                   .count        = _size_in_blocks };

		new (_alloc) Job(*_block, operation, _job_cnt);
	}

	Result result() override
	{
		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, _size, _info.block_size, _triggered);
	}

	char const *name() const override { return "random"; }

	void print(Output &out) const override
	{
		Genode::print(out, name(),             " "
		                   "size:",   _size,   " "
		                   "length:", _length, " "
		                   "copy:",   _copy,   " "
		                   "batch:",  _batch);
	}
};

#endif /* _TEST_RANDOM_H_ */
