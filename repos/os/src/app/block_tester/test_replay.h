/*
 * \brief  Block session testing
 * \author Josef Soentgen
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TEST_REPLAY_H_
#define _TEST_REPLAY_H_

namespace Test { struct Replay; }

/*
 * Replay test
 *
 * This test replays a recorded sequence of Block session requests.
 */
struct Test::Replay : Test_base
{
	using Test_base::Test_base;

	void _init() override
	{
		try {
			_node.for_each_sub_node("request", [&](Xml_node request) {

				auto op_type = [&] ()
				{
					typedef String<8> Type;

					if (request.attribute_value("type", Type()) == "read")
						return Block::Operation::Type::READ;

					if (request.attribute_value("type", Type()) == "write")
						return Block::Operation::Type::WRITE;

					if (request.attribute_value("type", Type()) == "sync")
						return Block::Operation::Type::SYNC;

					error("operation type not defined: ", request);
					throw 1;
				};

				Block::Operation const operation
				{
					.type         = op_type(),
					.block_number = request.attribute_value("lba", (block_number_t)0),
					.count        = request.attribute_value("count", 0UL)
				};

				_job_cnt++;
				new (&_alloc) Job(*_block, operation, _job_cnt);
			});
		} catch (...) {
			error("could not read request list");

			_block->dissolve_all_jobs([&] (Job &job) { destroy(_alloc, &job); });
			return;
		}
	}

	void _spawn_job() override { }

	Result result() override
	{
		return Result(_success, _end_time - _start_time,
		              _bytes, _rx, _tx, 0u, _info.block_size, _triggered);
	}

	char const *name() const override { return "replay"; }

	void print(Output &out) const override
	{
		Genode::print(out, name());
	}
};

#endif /* _TEST_REPLAY_H_ */
