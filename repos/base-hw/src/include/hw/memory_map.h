/*
 * \brief  Memory map of core
 * \author Stefan Kalkowski
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__MEMORY_MAP_H_
#define _SRC__LIB__HW__MEMORY_MAP_H_

#include <hw/mapping.h>
#include <hw/memory_region.h>
#include <hw/util.h>

namespace Hw {
	struct Mmio_space;

	namespace Mm {
		Memory_region const user();
		Memory_region const core_utcb_main_thread();
		Memory_region const core_stack_area();
		Memory_region const core_page_tables();
		Memory_region const core_mmio();
		Memory_region const core_heap();
		Memory_region const system_exception_vector();
		Memory_region const hypervisor_exception_vector();
		Memory_region const hypervisor_stack();
		Memory_region const supervisor_exception_vector();
		Memory_region const boot_info();
	}
}


struct Hw::Mmio_space : Hw::Memory_region_array
{
	using Hw::Memory_region_array::Memory_region_array;

	template <typename FUNC>
	void for_each_mapping(FUNC f) const
	{
		addr_t virt_base = Mm::core_mmio().base;
		auto lambda = [&] (unsigned, Memory_region const & r) {
			f(Mapping { r.base, virt_base, r.size, Genode::PAGE_FLAGS_KERN_IO });
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

#endif /* _SRC__LIB__HW__MEMORY_MAP_H_ */
