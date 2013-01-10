/*
 * \brief  RM-session implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
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
#include <base/ipc_pager.h>

/* core includes */
#include <rm_session_component.h>
#include <nova_util.h>

using namespace Genode;

static const bool verbose = false;

void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	addr_t const core_local_end = core_local_base + (size - 1);
	off_t const  core_to_virt   = virt_base - core_local_base;

	Nova::Rights rwx(true, true, true);

	while (true) {
		Nova::Mem_crd crd(core_local_base >> 12, 32, rwx);
		Nova::lookup(crd);

		if (!crd.is_null()) {

			if (verbose)
				PINF("Unmapping local: %08lx virt: %08lx base: %lx order: %lx size: %lx is null: %d",
				     core_local_base, core_local_base + core_to_virt, crd.base(), crd.order(),
				     (0x1000UL << crd.order()), crd.is_null());

			unmap_local(crd, false);

			core_local_base = (crd.base() << 12)       /* base address of mapping */
			                + (0x1000 << crd.order()); /* size of mapping */
		} else {

			/* This can happen if the region has never been touched */

			if (verbose)
				PINF("Nothing mapped at local: %08lx virt: %08lx",
					core_local_base, core_local_base + core_to_virt);

			core_local_base += 0x1000;
		}

		if (core_local_base > core_local_end)
			return;
	}
}
