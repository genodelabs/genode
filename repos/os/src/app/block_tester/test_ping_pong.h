/*
 * \brief  Block session testing - ping pong test
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_PING_PONG_H_
#define _TEST_PING_PONG_H_

namespace Test { struct Ping_pong; }


/*
 * Ping_pong operation test
 *
 * This test reads or writes the given number of blocks from the
 * specified start block sequentially in sized requests.
 */
struct Test::Ping_pong : Test_base
{
	bool           _ping   = true;
	block_number_t _end    = 0;
	block_number_t _start  = _node.attribute_value("start",  0u);
	size_t   const _size   = _node.attribute_value("size",   Number_of_bytes());
	size_t   const _length = _node.attribute_value("length", Number_of_bytes());

	Block::Operation::Type const _op_type = _node.attribute_value("write", false)
	                                      ? Block::Operation::Type::WRITE
	                                      : Block::Operation::Type::READ;

	using Test_base::Test_base;

	void _spawn_job() override
	{
		if (_bytes >= _length)
			return;

		_job_cnt++;

		block_number_t const lba = _ping ? _start : _end - _start;
		_ping = !_ping;

		Block::Operation const operation { .type         = _op_type,
		                                   .block_number = lba,
		                                   .count        = _size_in_blocks };

		new (_alloc) Job(*_block, operation, _job_cnt);

		_start += _size_in_blocks;
	}

	void _init() override
	{
		if (_size > _scratch_buffer.size) {
			error("request size exceeds scratch buffer size");
			throw Constructing_test_failed();
		}

		size_t const total_bytes = _info.block_count * _info.block_size;
		if (_length > total_bytes - (_start * _info.block_size)) {
			error("length too large invalid");
			throw Constructing_test_failed();
		}

		if (_info.block_size > _size || (_size % _info.block_size) != 0) {
			error("request size invalid");
			throw Constructing_test_failed();
		}

		_size_in_blocks   = _size   / _info.block_size;
		_length_in_blocks = _length / _info.block_size;
		_end              = _start  + _length_in_blocks;
	}

	Result result() override
	{
		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, _size, _info.block_size, _triggered);
	}

	char const *name() const override { return "ping_pong"; }

	void print(Output &out) const override
	{
		Genode::print(out, name(), " ", Block::Operation::type_name(_op_type), " "
		                   "start:",  _start,  " "
		                   "size:",   _size,   " "
		                   "length:", _length, " "
		                   "copy:",   _copy,   " "
		                   "batch:",  _batch);
	}
};

#endif /* _TEST_PING_PONG_H_ */
