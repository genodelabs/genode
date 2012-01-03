/*
 * \brief  Fiasco-specific part of RM-session implementation
 * \author Stefan Kalkowski
 * \date   2011-01-18
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <rm_session_component.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/task.h>
}

using namespace Genode;


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	using namespace Fiasco;

	// TODO unmap it only from target space
	addr_t addr = core_local_base;
	for (; addr < core_local_base + size; addr += L4_PAGESIZE)
		l4_task_unmap(L4_BASE_TASK_CAP,
		              l4_fpage(addr, L4_LOG2_PAGESIZE, L4_FPAGE_RW),
		              L4_FP_OTHER_SPACES);
}
