/*
 * \brief  Pistachio-specific implementation of the IO_MEM session interface
 * \author Julian Stecklina
 * \date   2008-04-09
 *
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>
#include <kip.h>

/* base-internal includes */
#include <base/internal/pistachio.h>

using namespace Core;


/*
 * TODO This should take a size parameter and check if the whole
 *      region is "normal" memory.
 */
static bool is_conventional_memory(addr_t base)
{
	using namespace Pistachio;
	L4_KernelInterfacePage_t *kip = get_kip();

	/* I miss useful programming languages... */
	for (L4_Word_t i = 0; i < L4_NumMemoryDescriptors(kip); i++) {
		L4_MemoryDesc_t *d = L4_MemoryDesc(kip, i);

		if (!L4_IsVirtual(d) && (L4_Type(d) == 1))
			if ((L4_Low(d) <= base) && (base <= L4_High(d)))
				return true;
	}

	return false;
}


void Io_mem_session_component::_unmap_local(addr_t, size_t, addr_t) { }


static inline bool can_use_super_page(addr_t base, size_t size)
{
	return (base & (get_super_page_size() - 1)) == 0
	    && (size >= get_super_page_size());
}


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	using namespace Pistachio;

	auto alloc_virt_range = [&] ()
	{
		/* special case for the null page */
		if (is_conventional_memory(base))
			return base;

		/* align large I/O dataspaces to super page size, otherwise to size */
		size_t const align = (size >= get_super_page_size())
		                           ? get_super_page_size_log2()
		                           : log2(size);

		return platform().region_alloc().alloc_aligned(size, align).convert<addr_t>(
			[&] (void *ptr) { return (addr_t)ptr; },
			[&] (Range_allocator::Alloc_error) -> addr_t {
				error(__func__, ": alloc_aligned failed!");
				return 0; });
	};

	addr_t const local_base = (addr_t)alloc_virt_range();

	if (!local_base)
		return 0;

	for (unsigned offset = 0; size; ) {

		size_t page_size = get_page_size();
		if (can_use_super_page(base + offset, size))
			page_size = get_super_page_size();

		L4_Sigma0_GetPage_RcvWindow(get_sigma0(),
		                            L4_Fpage(base       + offset, page_size),
		                            L4_Fpage(local_base + offset, page_size));

		if (_cacheable == WRITE_COMBINED) {
			int res = L4_Set_PageAttribute(L4_Fpage(local_base + offset, page_size),
			                               L4_WriteCombiningMemory);
			if (res != 1)
				error(__func__, ": L4_Set_PageAttributes virt returned ", res);
		}

		offset += page_size;
		size   -= page_size;
	}

	return local_base;
}
