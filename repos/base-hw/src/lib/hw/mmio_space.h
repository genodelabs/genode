/*
 * \brief  Representation of MMIO space
 * \author Stefan Kalkowski
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__MMIO_SPACE_H_
#define _SRC__LIB__HW__MMIO_SPACE_H_

#include <hw/mapping.h>
#include <hw/memory_region.h>
#include <hw/util.h>

namespace Hw { struct Mmio_space; }

struct Hw::Mmio_space : Hw::Memory_region_array
{
	using Hw::Memory_region_array::Memory_region_array;

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
		 * Sadly this method is used quite early during bootstrap
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

#endif /* _SRC__LIB__HW__MMIO_SPACE_H_ */
