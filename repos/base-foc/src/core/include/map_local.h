/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \author Stefan Kalkowski
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

/* core includes */
#include <platform.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sigma0/sigma0.h>
#include <l4/sys/task.h>
#include <l4/sys/cache.h>
}

namespace Genode {

	/**
	 * Map pages locally within core
	 *
	 * On Fiasco, all mapping originate from virtual addresses. At startup,
	 * core obtains the whole memory sigma0 in a one-to-one fashion. Hence,
	 * core-local addresses normally correspond to physical addresses.
	 *
	 * \param from_addr  core-virtual source address
	 * \param to_addr    core-virtual destination address
	 * \param num_pages  number of pages to remap
	 */
	inline bool map_local(addr_t from_addr, addr_t to_addr, size_t num_pages)
	{
		addr_t offset         = 0;
		size_t page_size      = get_page_size();
		size_t page_size_log2 = get_page_size_log2();
		for (unsigned i = 0; i < num_pages; i++, offset += page_size) {

			using namespace Fiasco;

			l4_fpage_t snd_fpage = l4_fpage(from_addr + offset,
			                                page_size_log2, L4_FPAGE_RW);

			if (l4_msgtag_has_error(l4_task_map(L4_BASE_TASK_CAP,
			                                    L4_BASE_TASK_CAP,
			                                    snd_fpage,
			                                    to_addr + offset))) {
				warning("could not locally remap ", (void*)from_addr, " to ",
				        (void*)to_addr);
				return false;
			}
		}

		return true;
	}


	static inline bool can_use_super_page(addr_t base, size_t size)
	{
		return (base & (get_super_page_size() - 1)) == 0
		    && (size >= get_super_page_size());
	}


	/**
	 * Map memory-mapped I/O range within core
	 *
	 * \return true on success
	 */
	static inline bool map_local_io(addr_t from_addr, addr_t to_addr,
	                                size_t num_pages)
	{
		using namespace Fiasco;

		size_t size = num_pages << get_page_size_log2();

		/* call sigma0 for I/O region */
		unsigned offset = 0;
		while (size) {
			/* FIXME what about caching demands? */
			/* FIXME what about read / write? */

			l4_utcb_mr()->mr[0] = SIGMA0_REQ_FPAGE_IOMEM;

			size_t page_size_log2 = get_page_size_log2();
			if (can_use_super_page(from_addr + offset, size))
				page_size_log2 = get_super_page_size_log2();
			l4_utcb_mr()->mr[1] = l4_fpage(from_addr + offset,
			                               page_size_log2, L4_FPAGE_RWX).raw;

			/* open receive window for mapping */
			l4_utcb_br()->bdr   = 0;
			l4_utcb_br()->br[0] = L4_ITEM_MAP;
			l4_utcb_br()->br[1] = l4_fpage((addr_t)to_addr + offset,
			                               page_size_log2, L4_FPAGE_RWX).raw;

			l4_msgtag_t tag = l4_msgtag(L4_PROTO_SIGMA0, 2, 0, 0);
			tag = l4_ipc_call(L4_BASE_PAGER_CAP, l4_utcb(), tag, L4_IPC_NEVER);
			if (l4_ipc_error(tag, l4_utcb())) {
				error("Ipc error ", l4_ipc_error(tag, l4_utcb()));
				return false;
			}

			if (l4_msgtag_items(tag) < 1) {
				error("got no mapping!");
				return false;
			}

			offset += 1 << page_size_log2;
			size   -= 1 << page_size_log2;
		}
		return true;
	}


	static inline void unmap_local(addr_t const local_base, size_t const num_pages)
	{
		using namespace Fiasco;

		size_t const size = num_pages << get_page_size_log2();
		addr_t addr = local_base;

		/*
		 * XXX divide operation into flexpages greater than page size
		 */
		for (; addr < local_base + size; addr += L4_PAGESIZE)
			l4_task_unmap(L4_BASE_TASK_CAP,
			              l4_fpage(addr, L4_LOG2_PAGESIZE, L4_FPAGE_RW),
			              L4_FP_OTHER_SPACES);

		l4_cache_dma_coherent(local_base, local_base + size);
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
