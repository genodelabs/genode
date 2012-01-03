/*
 * \brief  OKL4-specific part of RM-session implementation
 * \author Norman Feske
 * \date   2009-04-10
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <rm_session_component.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/types.h>
#include <l4/space.h>
#include <l4/ipc.h>
} }

using namespace Genode;
using namespace Okl4;

static const bool verbose_unmap = false;


static void unmap_log2_range(L4_SpaceId_t space_id, L4_Word_t base, L4_Word_t size_log2)
{
	L4_Fpage_t fpage = L4_FpageLog2(base, size_log2);
	L4_FpageAddRightsTo(&fpage, L4_FullyAccessible);
	int ret = L4_UnmapFpage(space_id, fpage);
	if (ret != 1)
		PERR("could not unmap page at %p from space %lx (Error Code %ld)",
		     (void *)base, space_id.raw, L4_ErrorCode());
}


void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	L4_ThreadId_t tid            = { raw : badge() };
	L4_SpaceId_t  space_id       = { raw: L4_ThreadNo(tid) >> Thread_id_bits::THREAD };
	L4_Word_t     addr           = virt_base;
	L4_Word_t     remaining_size = size;
	L4_Word_t     size_log2      = get_page_size_log2();

	if (verbose_unmap)
		printf("RM client %p (%lx) unmap [%lx,%lx)\n",
		       this, badge(), virt_base, virt_base + size);

	/*
	 * Let unmap granularity ('size_log2') grow
	 */
	while (remaining_size >= (1UL << size_log2)) {

		enum { SIZE_LOG2_MAX = 22 /* 4M */ };

		/* issue 'unmap' for the current address if flexpage aligned */
		if (addr & (1 << size_log2)) {
			unmap_log2_range(space_id, addr, size_log2);

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
			unmap_log2_range(space_id, addr, size_log2);

			remaining_size -= 1 << size_log2;
			addr           += 1 << size_log2;
		}

		/* decrease flexpage size */
		size_log2--;
	}
}
