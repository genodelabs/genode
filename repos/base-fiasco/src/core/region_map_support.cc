/*
 * \brief  Fiasco-specific part of RM-session implementation
 * \author Norman Feske
 * \date   2009-04-10
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <rm_session_component.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/syscalls.h>
#include <l4/sys/types.h>
}

using namespace Genode;

static const bool verbose_unmap = false;


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	/*
	 * Fiasco's 'unmap' syscall unmaps the specified flexpage from all address
	 * spaces to which we mapped the pages. We cannot target this operation to
	 * a specific L4 task. Hence, we unmap the dataspace from all tasks, not
	 * only for this RM client.
	 */
	if (verbose_unmap) {
		Fiasco::l4_threadid_t tid; tid.raw = badge();
		log("RM client ", this, " (", (unsigned)tid.id.task, ".",
			(unsigned)tid.id.lthread, ") unmap core-local [",
			Hex(core_local_base), ",", Hex(core_local_base + size), ")");
	}

	using namespace Fiasco;

	addr_t addr = core_local_base;
	for (; addr < core_local_base + size; addr += L4_PAGESIZE)
		l4_fpage_unmap(l4_fpage(addr, L4_LOG2_PAGESIZE, 0, 0),
		               L4_FP_FLUSH_PAGE);
}
