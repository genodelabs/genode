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

#include <types.h>

namespace Test { struct Replay; }

/*
 * Replay test
 *
 * This test replays a recorded sequence of Block session requests.
 */
struct Test::Replay : Scenario
{
	Allocator &_alloc;

	unsigned _next_id = 0;
	unsigned _count   = 0;

	struct Step : Id_space<Step>::Element
	{
		Block::Operation operation;
		Step(Id_space<Step> &steps, unsigned id, Block::Operation operation)
		:
			Id_space<Step>::Element(*this, steps, { id }), operation(operation)
		{ }
	};

	Id_space<Step> _steps { };

	Replay(Allocator &alloc, Node const &node) : Scenario(node), _alloc(alloc)
	{
		node.for_each_sub_node("request", [&] (Node const &request) {

			struct Invalid { };

			auto op_type = [&] () -> Attempt<Block::Operation::Type, Invalid>
			{
				using Type = String<8>;

				if (request.attribute_value("type", Type()) == "read")
					return Block::Operation::Type::READ;

				if (request.attribute_value("type", Type()) == "write")
					return Block::Operation::Type::WRITE;

				if (request.attribute_value("type", Type()) == "sync")
					return Block::Operation::Type::SYNC;

				return Invalid();
			};

			op_type().with_result(
				[&] (Block::Operation::Type type) {
					new (_alloc) Step(_steps, _count++, {
						.type         = type,
						.block_number = request.attribute_value("lba", (block_number_t)0),
						.count        = request.attribute_value("count", 0UL)
					});
				},
				[&] (Invalid) { error("operation type not defined: ", request); });
		});
	}

	~Replay()
	{
		while (_steps.apply_any<Step>([&] (Step &step) {
			destroy(_alloc, &step); }));
	}

	bool init(Init_attr const &) override { return true; }

	Next_job_result next_job(Stats const &) override
	{
		return _steps.apply<Step const>(Id_space<Step>::Id { _next_id++ },
			[&] (Step const &step) -> Next_job_result {
				return step.operation; },
			[&] () -> Next_job_result {
				return No_job(); });
	}

	size_t request_size() const override { return 0; }

	char const *name() const override { return "replay"; }

	void print(Output &out) const override
	{
		Genode::print(out, name());
	}
};

#endif /* _TEST_REPLAY_H_ */
