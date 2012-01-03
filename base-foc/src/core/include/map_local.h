/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

/* core includes */
#include <platform.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}

namespace Genode {

	/**
	 * Map page locally within core
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
				PWRN("could not locally remap 0x%lx to 0x%lx", from_addr, to_addr);
				return false;
			}
		}

		return true;
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */

