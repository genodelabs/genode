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

namespace Test {
	struct Random;
}

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
			uint64_t z = (seed += __UINT64_C(0x9E3779B97F4A7C15));
			z = (z ^ (z >> 30)) * __UINT64_C(0xBF58476D1CE4E5B9);
			z = (z ^ (z >> 27)) * __UINT64_C(0x94D049BB133111EB);
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
	Genode::Env       &_env;
	Genode::Allocator &_alloc;

	Block::Packet_descriptor::Opcode _op {
		Block::Packet_descriptor::READ };
	bool _alternate_access { false };

	/* block session infos */
	enum { TX_BUF_SIZE = 4 * 1024 * 1024, };
	Genode::Allocator_avl _block_alloc { &_alloc };
	Genode::Constructible<Block::Connection> _block { };

	Genode::Constructible<Timer::Connection> _timer { };

	Util::Xoroshiro _random;

	size_t   _size   { 0 };
	uint64_t _length { 0 };
	char   _scratch_buffer[1u<<20] { };

	size_t   _blocks     { 0 };

	/* _synchronous controls bulk */
	bool _synchronous { false };

	Genode::Constructible<Timer::Periodic_timeout<Random>> _progress_timeout { };

	void _handle_progress_timeout(Genode::Duration)
	{
		Genode::log("progress: rx:", _rx, " tx:", _tx);
	}

	Block::sector_t _next_block()
	{
		uint64_t r = 0;
		do {
			r = _random.get() % _block_count;
		} while (r + _size_in_blocks > _block_count);

		return r;
	}

	void _handle_submit()
	{
		try {
			bool next = true;
			while (_blocks < _length_in_blocks && _block->tx()->ready_to_submit() && next) {

				Block::Packet_descriptor tmp =
					_block->tx()->alloc_packet(_size);

				Block::sector_t lba = _next_block();

				Block::Packet_descriptor::Opcode op =
					_alternate_access ? (lba & 0x1)
					                    ? Block::Packet_descriptor::WRITE
					                    : Block::Packet_descriptor::READ
					                  : _op;

				Block::Packet_descriptor p(tmp, op, lba, _size_in_blocks);

				bool const write = op == Block::Packet_descriptor::WRITE;

				/* simulate write */
				if (write) {
					char * const content = _block->tx()->packet_content(p);
					Genode::memcpy(content, _scratch_buffer, p.size());
				}

				if (_verbose) {
					Genode::log("submit: lba:", lba, " size:", _size,
					            " ", write ? "tx" : "rx");
				}

				_block->tx()->submit_packet(p);
				_blocks += _size_in_blocks;

				next = !_synchronous;
			}
		} catch (...) { }
	}

	void _handle_ack()
	{
		if (_finished) { return; }

		while (_block->tx()->ack_avail()) {

			Block::Packet_descriptor p = _block->tx()->get_acked_packet();

			if (!p.succeeded()) {
				Genode::error("processing ", p.block_number(), " ",
				              p.block_count(), " failed");

				if (_stop_on_error) { throw Test_failed(); }
				else                { _finish(); break; }
			}

			size_t                           const psize = p.size();
			size_t                           const count = psize / _block_size;
			Block::Packet_descriptor::Opcode const op    = p.operation();

			bool const read = op == Block::Packet_descriptor::READ;

			/* simulate read */
			if (read) {
				char * const content = _block->tx()->packet_content(p);
				Genode::memcpy(_scratch_buffer, content, p.size());
			}

			if (_verbose) {
				Genode::log("ack: lba:", p.block_number(), " size:", p.size(),
				            " ", read ? "rx" : "tx");
			}

			_rx += (op == Block::Packet_descriptor::READ)  * count;
			_tx += (op == Block::Packet_descriptor::WRITE) * count;

			_bytes += psize;
			_block->tx()->release_packet(p);
		}

		if (_bytes >= _length) {
			_success = true;
			_finish();
			return;
		}

		_handle_submit();
	}

	void _finish()
	{
		_end_time = _timer->elapsed_ms();

		Test_base::_finish();
	}

	Genode::Signal_handler<Random> _ack_sigh {
		_env.ep(), *this, &Random::_handle_ack };

	Genode::Signal_handler<Random> _submit_sigh {
		_env.ep(), *this, &Random::_handle_submit };

	Genode::Xml_node _node;

	/**
	 * Constructor
	 *
	 * \param block  Block session reference
	 * \param node   node containing the test configuration
	 */
	Random(Genode::Env &env, Genode::Allocator &alloc,
	           Genode::Xml_node node,
	           Genode::Signal_context_capability finished_sig)
	:
		Test_base(finished_sig), _env(env), _alloc(alloc),
		_random(node.attribute_value("seed", 42u)),
		_node(node)
	{
		_verbose = node.attribute_value("verbose", false);
	}

	/********************
	 ** Test interface **
	 ********************/

	void start(bool stop_on_error) override
	{
		_stop_on_error = stop_on_error;

		_block.construct(_env, &_block_alloc, TX_BUF_SIZE);

		_block->tx_channel()->sigh_ack_avail(_ack_sigh);
		_block->tx_channel()->sigh_ready_to_submit(_submit_sigh);

		_block->info(&_block_count, &_block_size, &_block_ops);

		try {
			Genode::Number_of_bytes tmp;
			_node.attribute("size").value(&tmp);
			_size = tmp;

			_node.attribute("length").value(&tmp);
			_length = tmp;
		} catch (...) { }

		if (_size > sizeof(_scratch_buffer)) {
			Genode::error("request size exceeds scratch buffer size");
			throw Constructing_test_failed();
		}

		if (!_size || !_length) {
			Genode::error("request size or length invalid");
			throw Constructing_test_failed();
		}

		if (_block_size > _size || (_size % _block_size) != 0) {
			Genode::error("request size invalid ", _block_size, " ", _size);
			throw Constructing_test_failed();
		}

		_synchronous = _node.attribute_value("synchronous", false);

		bool r = _node.attribute_value("write", false);
		if (r) { _op = Block::Packet_descriptor::WRITE; }

		bool w = _node.attribute_value("read", false);
		if (w) { _op = Block::Packet_descriptor::WRITE; }

		_alternate_access = w && r;

		_size_in_blocks   = _size   / _block_size;
		_length_in_blocks = _length / _block_size;

		_timer.construct(_env);

		uint64_t const progress_interval = _node.attribute_value("progress", 0ul);
		if (progress_interval) {
			_progress_timeout.construct(*_timer, *this,
			                            &Random::_handle_progress_timeout,
			                            Genode::Microseconds(progress_interval*1000));
		}

		_start_time = _timer->elapsed_ms();
		_handle_submit();
	}

	Result finish() override
	{
		_timer.destruct();
		_block.destruct();

		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, _size, _block_size);
	}

	char const *name() const { return "random"; }
};

#endif /* _TEST_RANDOM_H_ */
