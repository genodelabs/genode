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

#ifndef _TEST_SEQUENTIAL_H_
#define _TEST_SEQUENTIAL_H_

#include <types.h>

namespace Test { struct Sequential; }


/*
 * Sequential operation test
 *
 * This test reads or writes the given number of blocks from the
 * specified start block sequentially in sized requests.
 */
struct Test::Sequential : Scenario
{
	block_number_t _start;
	size_t   const _size;
	size_t   const _length;
	block_number_t _end = 0;

	Block::Operation::Type const _op_type;

	Operation_size _op_size          { };   /* assigned by init() */
	size_t         _length_in_blocks { };

	Sequential(Allocator &, Xml_node const &node)
	:
		Scenario(node),
		_start  (node.attribute_value("start",  0u)),
		_size   (node.attribute_value("size",   Number_of_bytes())),
		_length (node.attribute_value("length", Number_of_bytes())),
		_op_type(node.attribute_value("write", false) ? Block::Operation::Type::WRITE
		                                              : Block::Operation::Type::READ)
	{ }

	bool init(Init_attr const &attr) override
	{
		if (_size > attr.scratch_buffer_size) {
			error("request size exceeds scratch buffer size");
			return false;
		}

		if (attr.block_size > _size || (_size % attr.block_size) != 0) {
			error("request size invalid");
			return false;
		}

		_op_size          = { _size / attr.block_size };
		_length_in_blocks = _length / attr.block_size;
		_end              = _start + _length_in_blocks;

		if (_length == 0 || (_length % attr.block_size) != 0) {
			error("length attribute (", _length, ") must be a multiple of "
			      "block size (", attr.block_size, ")");
			return false;
		}

		return true;
	}

	Next_job_result next_job(Stats const &) override
	{
		if (_start >= _end)
			return No_job();

		Block::Operation const operation { .type         = _op_type,
		                                   .block_number = _start,
		                                   .count        = _op_size.blocks };
		_start += _op_size.blocks;

		return operation;
	}

	size_t request_size() const override { return _size; }

	char const *name() const override { return "sequential"; }

	void print(Output &out) const override
	{
		Genode::print(out, name(), " ", Block::Operation::type_name(_op_type), " "
		                   "start:",  _start, " "
		                   "size:",   Number_of_bytes(_size),   " "
		                   "length:", Number_of_bytes(_length), " "
		                   "copy:",   attr.copy, " "
		                   "batch:",  attr.batch);
	}
};

#endif /* _TEST_SEQUENTIAL_H_ */
