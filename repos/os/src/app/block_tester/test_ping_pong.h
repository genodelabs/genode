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

#include <types.h>

namespace Test { struct Ping_pong; }


/*
 * Ping_pong operation test
 *
 * This test reads or writes the given number of blocks from the
 * specified start block sequentially in an alternating fashion
 * from the beginning and the end of the session.
 */
struct Test::Ping_pong : Scenario
{
	bool           _ping = true;
	size_t         _block_size { };
	block_number_t _end = 0;
	block_number_t _start;
	size_t   const _size;
	size_t   const _length;

	Block::Operation::Type const _op_type;

	Ping_pong(Allocator &, Node const &node)
	:
		Scenario(node),
		_start  (node.attribute_value("start",  0u)),
		_size   (node.attribute_value("size",   Number_of_bytes())),
		_length (node.attribute_value("length", Number_of_bytes())),
		_op_type(node.attribute_value("write", false)
		       ? Block::Operation::Type::WRITE : Block::Operation::Type::READ)
	{ }

	bool init(Init_attr const &attr) override
	{
		_block_size = attr.block_size;

		if (_size > attr.scratch_buffer_size) {
			error("request size exceeds scratch buffer size");
			return false;
		}

		Total const total { attr.block_count.blocks * _block_size };
		if (_length > total.bytes - (_start * _block_size)) {
			error("length too large invalid");
			return false;
		}

		if (_block_size > _size || (_size % _block_size) != 0) {
			error("request size invalid");
			return false;
		}

		size_t const length_in_blocks = _length / _block_size;

		_end = _start + length_in_blocks;
		return true;
	}

	Next_job_result next_job(Stats const &stats) override
	{
		if (stats.total.bytes >= _length)
			return No_job();

		block_number_t const lba = _ping ? _start : _end - _start;
		_ping = !_ping;

		Block::Operation const operation { .type         = _op_type,
		                                   .block_number = lba,
		                                   .count        = _size / _block_size };
		_start += operation.count;

		return operation;
	}

	size_t request_size() const override { return _size; }

	char const *name() const override { return "ping_pong"; }

	void print(Output &out) const override
	{
		Genode::print(out, name(), " ", Block::Operation::type_name(_op_type), " "
		                   "start:",  _start,    " "
		                   "size:",   _size,     " "
		                   "length:", _length,   " "
		                   "copy:",   attr.copy, " "
		                   "batch:",  attr.batch);
	}
};

#endif /* _TEST_PING_PONG_H_ */
