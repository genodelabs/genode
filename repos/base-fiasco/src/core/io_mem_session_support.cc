/*
 * \brief  Fiasco-specific implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sigma0/sigma0.h>
}

using namespace Genode;


void Io_mem_session_component::_unmap_local(addr_t base, size_t size)
{
	platform()->region_alloc()->free(reinterpret_cast<void *>(base));
}


static inline bool can_use_super_page(addr_t base, size_t size) {
	return (base & (get_super_page_size() - 1)) == 0
	    && (size >= get_super_page_size()); }


addr_t Io_mem_session_component::_map_local(addr_t base, size_t size)
{
	using namespace Fiasco;

	/* align large I/O dataspaces on a super-page boundary within core */
	size_t alignment = (size >= get_super_page_size()) ? get_super_page_size_log2()
	                                                   : get_page_size_log2();

	/* find appropriate region for mapping */
	void *local_base = 0;
	if (platform()->region_alloc()->alloc_aligned(size, &local_base, alignment).error())
		return 0;

	/* call sigma0 for I/O region */
	int err;
	l4_umword_t request;
	l4_umword_t dw0, dw1;
	l4_msgdope_t result;
	l4_msgtag_t tag;

	l4_threadid_t sigma0 = sigma0_threadid;

	unsigned offset = 0;
	while (size) {
		/* FIXME what about caching demands? */
		/* FIXME what about read / write? */

		/* special case for page0, which is RAM in sigma0/x86 */
		if (base + offset == 0)
			request = SIGMA0_REQ_FPAGE_RAM;
		else
			request = SIGMA0_REQ_FPAGE_IOMEM;

		size_t page_size_log2 = get_page_size_log2();
		if (can_use_super_page(base + offset, size))
			page_size_log2 = get_super_page_size_log2();

		err = l4_ipc_call_tag(sigma0,
		                      L4_IPC_SHORT_MSG,
		                        request,
		                        l4_fpage(base + offset, page_size_log2, 0, 0).fpage,
		                      l4_msgtag(L4_MSGTAG_SIGMA0, 0, 0, 0),
		                      L4_IPC_MAPMSG((addr_t)local_base + offset, page_size_log2),
		                        &dw0, &dw1,
		                      L4_IPC_NEVER, &result, &tag);

		if (err || !l4_ipc_fpage_received(result)) {
			error("map_local failed err=", err, " "
			      "(", l4_ipc_fpage_received(result), ")");
			return 0;
		}

		offset += 1 << page_size_log2;
		size   -= 1 << page_size_log2;
	}

	return (addr_t)local_base;
}
