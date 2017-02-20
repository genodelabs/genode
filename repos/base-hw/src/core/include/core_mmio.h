/*
 * \brief  Representation of core's MMIO space
 * \author Stefan Kalkowski
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE_MMIO_H_
#define _CORE_MMIO_H_

#include <mapping.h>
#include <memory_region.h>
#include <util.h>

namespace Genode { struct Core_mmio; }

struct Genode::Core_mmio : Genode::Memory_region_array
{
	using Memory_region_array::Memory_region_array;

	struct Not_found {};

	template <typename FUNC>
	void for_each_mapping(FUNC f) const
	{
		addr_t virt_base = 0xf0000000UL; /* FIXME */
		auto lambda = [&] (Memory_region const & r) {
			f(Mapping { r.base, virt_base, r.size, PAGE_FLAGS_KERN_IO });
			virt_base += r.size + get_page_size();
		};
		for_each(lambda);
	}

	addr_t virt_addr(addr_t phys_addr) const
	{
		/*
		 * Sadly this method is used quite early in the kernel
		 * where no exceptions can be used
		 */
		addr_t ret = 0;
		for_each_mapping([&] (Mapping const & m)
		{
			if (phys_addr >= m.phys() && phys_addr < (m.phys()+m.size()))
				ret = m.virt() + (phys_addr-m.phys());
		});

		return ret;
	}
};

#endif /* _CORE_MMIO_H_ */
