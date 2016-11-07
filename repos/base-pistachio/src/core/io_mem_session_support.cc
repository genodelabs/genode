/*
 * \brief  Pistachio-specific implementation of the IO_MEM session interface
 * \author Julian Stecklina
 * \date   2008-04-09
 *
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>
#include <kip.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/ipc.h>
#include <l4/sigma0.h>
#include <l4/misc.h>
}

using namespace Genode;


/*
 * TODO This should take a size parameter and check if the whole
 *      region is "normal" memory.
 */
bool is_conventional_memory(addr_t base)
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


void Io_mem_session_component::_unmap_local(addr_t base, size_t size) { }


static inline bool can_use_super_page(addr_t base, size_t size) {
	return (base & (get_super_page_size() - 1)) == 0
	    && (size >= get_super_page_size()); }


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	using namespace Pistachio;

	addr_t local_base;

	/* align large I/O dataspaces on a super-page boundary within core */
	size_t alignment = (size >= get_super_page_size()) ? get_super_page_size_log2()
	                                                   : get_page_size_log2();

	/* special case for the null page */
	if (is_conventional_memory(base))
		local_base = base;

	else {

		/* find appropriate region for mapping */
		void *result = 0;
		if (platform()->region_alloc()->alloc_aligned(size, &result, alignment).error())
			error(__func__, ": alloc_aligned failed!");

		local_base = (addr_t)result;
	}

	unsigned offset = 0;
	while (size) {

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
