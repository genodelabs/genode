/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <util.h>

/* base-internal includes */
#include <base/internal/okl4.h>

namespace Genode {

	inline void unmap_local_log2_range(Okl4::L4_Word_t base, Okl4::L4_Word_t size_log2)
	{
		using namespace Okl4;
		L4_Fpage_t fpage = L4_FpageLog2(base, size_log2);
		L4_FpageAddRightsTo(&fpage, L4_FullyAccessible);
		int ret = L4_UnmapFpage(L4_rootspace, fpage);
		if (ret != 1)
			error("could not unmap page at ", Hex(base), " from core, "
			      "error=", L4_ErrorCode());
	}

	/**
	 * Map physical pages to core-local virtual address range
	 *
	 * On OKL4v2, all mappings originate from the physical address space.
	 *
	 * \param from_phys  physical source address
	 * \param to_virt    core-local destination address
	 * \param num_pages  number of pages to map
	 *
	 * \return true on success
	 */
	inline bool map_local(addr_t from_phys, addr_t to_virt, size_t num_pages)
	{
		using namespace Okl4;

		for (unsigned i = 0, offset = 0; i < num_pages; i++) {
			L4_Fpage_t fpage = L4_FpageLog2(to_virt + offset, get_page_size_log2());
			L4_PhysDesc_t phys_desc = L4_PhysDesc(from_phys + offset, 0);
			fpage.X.rwx = 7;

			if (L4_MapFpage(L4_rootspace, fpage, phys_desc) != 1) {
				error("core-local memory mapping failed, error=", L4_ErrorCode());
				return false;
			}
			offset += get_page_size();
		}
		return true;
	}

	/**
	 * Unmap pages from core's address space
	 *
	 * \param virt_addr  first core-local address to unmap, must be page-aligned
	 * \param num_pages  number of pages to unmap
	 *
	 * \return true on success
	 */
	inline bool unmap_local(addr_t virt_addr, size_t num_pages)
	{
		using namespace Okl4;

		L4_Word_t addr           = virt_addr;
		L4_Word_t remaining_size = num_pages << get_page_size_log2();
		L4_Word_t size_log2      = get_page_size_log2();

		/*
		 * Let unmap granularity ('size_log2') grow
		 */
		while (remaining_size >= (1UL << size_log2)) {

			enum { SIZE_LOG2_MAX = 22 /* 4M */ };

			/* issue 'unmap' for the current address if flexpage aligned */
			if (addr & (1 << size_log2)) {
				unmap_local_log2_range(addr, size_log2);

				remaining_size -= 1 << size_log2;
				addr           += 1 << size_log2;
			}

			/* increase flexpage size */
			size_log2++;
		}

		/*
		 * Let unmap granularity ('size_log2') shrink
		 */
		while (remaining_size > 0) {

			if (remaining_size >= (1UL << size_log2)) {
				unmap_local_log2_range(addr, size_log2);

				remaining_size -= 1 << size_log2;
				addr           += 1 << size_log2;
			}

			/* decrease flexpage size */
			size_log2--;
		}
		return true;
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
