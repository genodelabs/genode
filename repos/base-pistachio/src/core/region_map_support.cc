/*
 * \brief  Pistachio-specific part of region-map implementation
 * \author Norman Feske
 * \date   2009-04-10
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

/* core includes */
#include <rm_session_component.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/types.h>
#include <l4/space.h>
}

using namespace Genode;

void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	/*
	 * Pistachio's 'unmap' syscall unmaps the specified flexpage from all
	 * address spaces to which we mapped the pages. We cannot target this
	 * operation to a specific L4 task. Hence, we unmap the dataspace from
	 * all tasks, not only for this RM client.
	 */

	using namespace Pistachio;

	L4_Word_t page_size = get_page_size();

	addr_t addr = core_local_base;
	for (; addr < core_local_base + size; addr += page_size) {
		L4_Fpage_t fp = L4_Fpage(addr, page_size);
		L4_Unmap(L4_FpageAddRightsTo(&fp, L4_FullyAccessible));
	}
}
