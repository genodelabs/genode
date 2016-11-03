/*
 * \brief  Representation of physical to virtual memory mappings
 * \author Stefan Kalkowski
 * \date   2016-11-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MAPPING_H_
#define _MAPPING_H_

#include <base/output.h>
#include <page_flags.h>

namespace Genode { struct Mapping; }

struct Genode::Mapping
{
	addr_t     phys = 0;
	addr_t     virt = 0;
	size_t     size = 0;
	Page_flags flags;

	Mapping() {}

	Mapping(addr_t virt, addr_t phys, Cache_attribute cacheable,
	        bool io, unsigned size_log2, bool writeable)
	: phys(phys), virt(virt), size(1 << size_log2),
	  flags{ writeable, true, false, false, io, cacheable } {}

	Mapping(addr_t phys, addr_t virt, size_t size, Page_flags flags)
	: phys(phys), virt(virt), size(size), flags(flags) {}

	void print(Output & out) const
	{
		Genode::print(out, "phys=", (void*)phys, " => virt=", (void*) virt,
		              " (size=", Hex(size, Hex::PREFIX, Hex::PAD),
		              " page-flags: ", flags, ")");
	}

	/**
	 * Dummy implementation used by generic region_map code
	 */
	void prepare_map_operation() {}
};

#endif /* _MAPPING_H_ */
