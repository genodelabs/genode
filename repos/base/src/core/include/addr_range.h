/*
 * \brief  Memory-address range
 * \author Norman Feske
 * \date   2023-06-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ADDR_RANGE_H_
#define _CORE__INCLUDE__ADDR_RANGE_H_

/* core includes */
#include <types.h>

namespace Core { struct Addr_range; }


struct Core::Addr_range
{
	addr_t start;
	addr_t end;  /* last byte */

	bool valid() const { return end > start; }

	Addr_range intersected(Addr_range const &other) const
	{
		if (!valid() || !other.valid())
			return { };

		return { max(start, other.start), min(end, other.end) };
	}

	bool contains(addr_t at) const { return (at >= start) && (at <= end); }

	Addr_range reduced_by(addr_t offset) const
	{
		if (!valid() || (offset > start))
			return { };

		return Addr_range { start - offset, end - offset };
	}

	Addr_range increased_by(addr_t offset) const
	{
		if (!valid() || (offset + start < offset) || (offset + end < offset))
			return { };

		return Addr_range { start + offset, end + offset };
	}

	void print(Output &out) const
	{
		Genode::print(out, "[", Hex(start), ",", Hex(end), "]");
	}
};

#endif /* _CORE__INCLUDE__ADDR_RANGE_H_ */
