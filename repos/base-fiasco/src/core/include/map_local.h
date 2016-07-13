/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
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
		Fiasco::l4_threadid_t core_pager = platform_specific()->core_pager()->native_thread_id();

		addr_t offset = 0;
		size_t page_size = get_page_size();
		size_t page_size_log2 = get_page_size_log2();
		for (unsigned i = 0; i < num_pages; i++, offset += page_size) {

			using namespace Fiasco;

			/* perform echo request to the core pager */
			l4_umword_t  dummy = 0;
			l4_msgdope_t ipc_result;
			l4_fpage_t   from_fpage = l4_fpage(from_addr + offset,
			                                   page_size_log2, true, false);
			enum { ECHO_LOCAL_MAP_REQUEST = 0 };
			l4_ipc_call(core_pager, L4_IPC_SHORT_MSG,
			            from_fpage.raw,          /* normally page-fault addr */
			            ECHO_LOCAL_MAP_REQUEST,  /* normally page-fault IP */
			            L4_IPC_MAPMSG(to_addr + offset, page_size_log2),
			            &dummy, &dummy,
			            L4_IPC_NEVER, &ipc_result);

			if (L4_IPC_IS_ERROR(ipc_result)) {
				warning("could not locally remap ", Hex(from_addr), " "
				        "to ", Hex(to_addr), ", "
				        "error code is ", L4_IPC_ERROR(ipc_result));
				return false;
			}
		}
		return true;
	}

	/**
	 * Unmap pages locally within core
	 *
	 * \param virt       core-local address
	 * \param num_pages  number of pages to unmap
	 */
	inline void unmap_local(addr_t virt, size_t num_pages)
	{
		error("unmap_local() called - not implemented yet");
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
