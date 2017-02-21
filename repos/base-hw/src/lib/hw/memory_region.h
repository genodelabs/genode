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

#ifndef _SRC__LIB__HW__MEMORY_REGION_H_
#define _SRC__LIB__HW__MEMORY_REGION_H_

#include <base/output.h>
#include <hw/array.h>
#include <hw/util.h>

namespace Hw {
	struct Memory_region;
	using Memory_region_array = Array<Memory_region, 16>;
}

struct Hw::Memory_region
{
	Genode::addr_t base = 0;
	Genode::size_t size = 0;

	Memory_region(Genode::addr_t base, Genode::size_t size)
	: base(trunc(base, get_page_size_log2())),
	  size(round(size, get_page_size_log2())) {}

	Memory_region() {}

	Genode::addr_t end() const { return base + size; }

	void print(Genode::Output & out) const
	{
		Genode::print(out, "base=", (void*)base, " size=",
		              Genode::Hex(size, Genode::Hex::PREFIX));
	}
};

#endif /* _SRC__LIB__HW__MEMORY_REGION_H_ */
