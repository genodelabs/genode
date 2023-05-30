/*
 * \brief  Fiasco-specific implementation of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-08-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <util.h>
#include <io_mem_session_component.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Core;


void Io_mem_session_component::_unmap_local(addr_t base, size_t, addr_t)
{
	platform().region_alloc().free(reinterpret_cast<void *>(base));
}


static inline bool can_use_super_page(addr_t, size_t)
{
	/*
	 * We disable super-page I/O mappings as unmap does not flush the local
	 * mapping which breaks later re-mappings of different page size.
	 */
	return false;
}


addr_t Io_mem_session_component::_map_local(addr_t phys_base, size_t size)
{
	auto map_io_region = [] (addr_t phys_base, addr_t local_base, size_t size)
	{
		using namespace Fiasco;

		l4_threadid_t const sigma0 = sigma0_threadid;

		unsigned offset = 0;
		while (size) {

			/*
			 * Call sigma0 for I/O region
			 */

			/* special case for page0, which is RAM in sigma0/x86 */
			l4_umword_t const request = (phys_base + offset == 0)
			                          ? SIGMA0_REQ_FPAGE_RAM
			                          : SIGMA0_REQ_FPAGE_IOMEM;

			size_t const size_log2 = can_use_super_page(phys_base + offset, size)
			                       ? get_super_page_size_log2()
			                       : get_page_size_log2();

			l4_umword_t  dw0 = 0, dw1 = 0;
			l4_msgdope_t result { };
			l4_msgtag_t  tag    { };

			int const err =
				l4_ipc_call_tag(sigma0,
				                L4_IPC_SHORT_MSG,
				                request,
				                l4_fpage(phys_base + offset, size_log2, 0, 0).fpage,
				                l4_msgtag(L4_MSGTAG_SIGMA0, 0, 0, 0),
				                L4_IPC_MAPMSG(local_base + offset, size_log2),
				                               &dw0, &dw1,
				                L4_IPC_NEVER, &result, &tag);

			if (err || !l4_ipc_fpage_received(result)) {
				error("map_local failed err=", err, " "
				      "(", l4_ipc_fpage_received(result), ")");
				return;
			}

			offset += 1 << size_log2;
			size   -= 1 << size_log2;
		}
	};

	/* align large I/O dataspaces on a super-page boundary within core */
	size_t align = (size >= get_super_page_size()) ? get_super_page_size_log2()
	                                               : get_page_size_log2();

	return platform().region_alloc().alloc_aligned(size, align).convert<addr_t>(

		[&] (void *ptr) {
			addr_t const core_local_base = (addr_t)ptr;
			map_io_region(phys_base, core_local_base, size);
			return core_local_base; },

		[&] (Range_allocator::Alloc_error) -> addr_t {
			error("core-local mapping of memory-mapped I/O range failed");
			return 0; });
}
