/*
 * \brief  Common types used by the block tester
 * \author Norman Feske
 * \date   2025-03-25
 */

/*
 * Copyright (C) 2016-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <base/heap.h>
#include <base/log.h>

namespace Test {

	using namespace Genode;

	using block_number_t = Block::block_number_t;

	struct Block_count { Block::block_number_t blocks; };

	struct Operation_size { Block::block_count_t blocks; };

	struct Total
	{
		uint64_t bytes;

		void print(Output &out) const { Number_of_bytes::print(out, bytes); }
	};

	struct Stats
	{
		Total rx, tx;
		Total total;
		unsigned completed;
		unsigned job_cnt;
	};

	struct Scenario;
}


struct Test::Scenario : Interface, private Fifo<Scenario>::Element
{
	friend class Fifo<Scenario>;

	struct Attr
	{
		size_t   io_buffer;
		uint64_t progress_interval;
		size_t   batch;
		bool     copy;
		bool     verbose;

		static Attr from_xml(Xml_node const &node)
		{
			return {
				.io_buffer         = node.attribute_value("io_buffer", Number_of_bytes(4*1024*1024)),
				.progress_interval = node.attribute_value("progress", (uint64_t)0),
				.batch             = node.attribute_value("batch",   1u),
				.copy              = node.attribute_value("copy",    true),
				.verbose           = node.attribute_value("verbose", false),
			};
		}
	};

	Attr const attr;

	Scenario(Xml_node const &node) : attr(Attr::from_xml(node)) { }

	struct Init_attr
	{
		size_t      block_size;        /* size of one block in bytes */
		Block_count block_count;
		size_t      scratch_buffer_size;
	};

	[[nodiscard]] virtual bool init(Init_attr const &) = 0;

	struct No_job { };
	using Next_job_result = Attempt<Block::Operation, No_job>;
	virtual Next_job_result next_job(Stats const &) = 0;

	virtual size_t request_size() const = 0;
	virtual char const *name() const = 0;
	virtual void print(Output &) const = 0;
};

#endif /* _TYPES_H_ */
