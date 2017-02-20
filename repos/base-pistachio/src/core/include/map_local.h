/*
 * \brief  Core-local mapping
 * \author Norman Feske
 * \date   2010-02-15
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAP_LOCAL_H_
#define _CORE__INCLUDE__MAP_LOCAL_H_

#include <base/printf.h>

/* core includes */
#include <platform.h>
#include <util.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/space.h>
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/kdebug.h>
}

namespace Genode {

	/**
	 * Map page locally within core
	 *
	 * On Pistachio, all mapping originate from virtual addresses. At startup,
	 * core obtains the whole memory sigma0 in a one-to-one fashion. Hence,
	 * core-local addresses normally correspond to physical addresses.
	 *
	 * \param from_addr  core-virtual source address
	 * \param to_addr    core-virtual destination address
	 * \param num_pages  number of pages to remap
	 */
	inline static bool map_local(addr_t from_addr, addr_t to_addr, size_t num_pages)
	{

		Pistachio::L4_ThreadId_t core_pager = platform_specific()->core_pager()->native_thread_id();

		addr_t offset = 0;
		size_t page_size = get_page_size();
		for (unsigned i = 0; i < num_pages; i++, offset += page_size) {

			using namespace Pistachio;

			L4_Fpage_t fpage = L4_Fpage(from_addr + offset, page_size);
			fpage += L4_FullyAccessible;
			L4_MapItem_t map_item = L4_MapItem(fpage, 0);

			/* assemble local echo mapping request */
			L4_Msg_t msg;
			L4_Word_t echo_request = 0, item_addr = (addr_t)&map_item;
			L4_Clear(&msg);
			L4_Append(&msg, item_addr);
			L4_Append(&msg, echo_request);
			msg.tag.X.u = 2;

			/* setup receive window */
			L4_Fpage_t rcv_fpage = L4_Fpage(to_addr + offset, page_size);
			L4_Accept(L4_MapGrantItems(rcv_fpage));

			L4_Load(&msg);

			L4_MsgTag_t result = L4_Call(core_pager);
			if (L4_IpcFailed(result)) {
				warning("could not locally remap ", (void*)from_addr, " to ",
				        (void*)to_addr, ", error code is ", L4_ErrorCode());
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
		size_t page_size = get_page_size();
		addr_t offset = 0;
		for (unsigned i = 0; i < num_pages; i++, offset += page_size) {
			using namespace Pistachio;
			L4_Fpage_t fpage = L4_Fpage(virt + offset, page_size);
			fpage += L4_FullyAccessible;
			L4_Flush(fpage);
		}
	}
}

#endif /* _CORE__INCLUDE__MAP_LOCAL_H_ */
