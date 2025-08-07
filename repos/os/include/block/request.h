/*
 * \brief  Block request
 * \author Norman Feske
 * \date   2018-12-17
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK__REQUEST_H_
#define _INCLUDE__BLOCK__REQUEST_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>
#include <util/arg_string.h>

namespace Block {

	using block_number_t = Genode::uint64_t;
	using block_count_t  = Genode::size_t;
	using off_t          = Genode::off_t;
	using seek_off_t     = Genode::uint64_t;

	struct Constrained_view;
	struct Operation;
	struct Request;
}


struct Block::Constrained_view
{
	struct Offset     { Genode::uint64_t value; };
	struct Num_blocks { Genode::uint64_t value; };

	Offset     offset;
	Num_blocks num_blocks;
	bool       writeable;

	static Constrained_view from_args(char const *args)
	{
		using AS = Genode::Arg_string;
		return {
			.offset     = Offset     { AS::find_arg(args, "offset")    .ulonglong_value(0) },
			.num_blocks = Num_blocks { AS::find_arg(args, "num_blocks").ulonglong_value(0) },\
			/*
			 * Assume writeable by default to accommodate session requests without
			 * contraining view because the policy's writeable setting always has
			 * the last word anyway.
			 */
			.writeable  = AS::find_arg(args, "writeable").bool_value(true)
		};
	}
};


struct Block::Operation
{
	enum class Type { INVALID = 0, READ = 1, WRITE = 2, SYNC = 3, TRIM = 4 };

	Type           type;
	block_number_t block_number;
	block_count_t  count;

	bool valid() const
	{
		return type == Type::READ || type == Type::WRITE
		    || type == Type::SYNC || type == Type::TRIM;
	}

	static bool has_payload(Type type)
	{
		return type == Type::READ || type == Type::WRITE;
	}

	static char const *type_name(Type type)
	{
		switch (type) {
		case Type::INVALID: return "INVALID";
		case Type::READ:    return "READ";
		case Type::WRITE:   return "WRITE";
		case Type::SYNC:    return "SYNC";
		case Type::TRIM:    return "TRIM";
		}
		return "INVALID";
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, type_name(type), " block=", block_number, " count=", count);
	}
};


struct Block::Request
{
	struct Tag { unsigned long value; };

	Operation operation;

	bool success;

	/**
	 * Location of payload within the packet stream
	 */
	off_t offset;

	/**
	 * Client-defined identifier to associate acknowledgements with requests
	 *
	 * The underlying type corresponds to 'Id_space::Id'.
	 */
	Tag tag;
};

#endif /* _INCLUDE__BLOCK__REQUEST_H_ */
