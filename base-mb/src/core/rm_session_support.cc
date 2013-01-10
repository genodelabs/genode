/*
 * \brief  RM-session implementation
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <rm_session_component.h>
#include <platform_thread.h>

using namespace Genode;

static bool const verbose = false;

void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	if (verbose) {
		PDBG("Flush %i B from [%p,%p)",
		     (unsigned int)size,
		     (void*)virt_base,
		     (void*)((unsigned int)virt_base+size));
	}

	Kernel::Protection_id pid =
		tid_allocator()->holder(this->badge())->pid();

	Kernel::tlb_flush(pid, virt_base, (unsigned int)size);
}
