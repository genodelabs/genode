/*
 * \brief   Memory information
 * \author  Stefan Kalkowski
 * \date    2016-09-30
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MEMORY_REGION_H_
#define _MEMORY_REGION_H_

#include <base/output.h>
#include <array.h>
#include <util.h>

namespace Genode {
	struct Memory_region;

	using Memory_region_array = Array<Memory_region, 16>;
}

struct Genode::Memory_region
{
	addr_t base = 0;
	size_t size = 0;

	Memory_region(addr_t base, size_t size)
	: base(trunc(base, get_page_size_log2())),
	  size(round(size, get_page_size_log2())) {}

	Memory_region() {}

	addr_t end() const { return base + size; }

	void print(Output & out) const
	{
		Genode::print(out, "base=", (void*)base,
		                   " size=", Hex(size, Hex::PREFIX));
	}
};

#endif /* _MEMORY_REGION_H_ */
