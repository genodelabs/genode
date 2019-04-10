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

namespace Block {

	typedef Genode::uint64_t block_number_t;
	typedef Genode::size_t   block_count_t;
	typedef Genode::off_t    off_t;

	struct Operation;
	struct Request;
}


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
