/*
 * \brief  Utilities fo thread creation on seL4
 * \author Norman Feske
 * \date   2015-05-12
 *
 * This file is used by both the core-specific implementation of the Thread API
 * and the platform-thread implementation for managing threads outside of core.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__THREAD_SEL4_H_
#define _CORE__INCLUDE__THREAD_SEL4_H_

/* base-internal includes */
#include <base/internal/assert.h>

/* core includes */
#include <map_local.h>
#include <kernel_object.h>
#include <untyped_memory.h>

namespace Genode {

	struct Thread_info
	{
		Cap_sel tcb_sel { 0 };
		Cap_sel ep_sel  { 0 };
		Cap_sel lock_sel { 0 };

		addr_t ipc_buffer_phys = 0;

		inline void write_thread_info_to_ipc_buffer(Cap_sel pd_ep_sel);

		Thread_info() { }

		inline void init(addr_t const utcb_virt_addr);
		inline void destruct();

	};

	/**
	 * Set register values for the instruction pointer and stack pointer and
	 * start the seL4 thread
	 */
	static inline void start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp);
};


void Genode::Thread_info::init(addr_t const utcb_virt_addr)
{
	Platform &platform = *platform_specific();
	Range_allocator &phys_alloc = *platform.ram_alloc();

	/* create IPC buffer of one page */
	ipc_buffer_phys = Untyped_memory::alloc_page(phys_alloc);
	Untyped_memory::convert_to_page_frames(ipc_buffer_phys, 1);

	/* allocate TCB within core's CNode */
	tcb_sel = platform.core_sel_alloc().alloc();
	create<Tcb_kobj>(phys_alloc, platform.core_cnode().sel(), tcb_sel);

	/* allocate synchronous endpoint within core's CNode */
	ep_sel = platform.core_sel_alloc().alloc();
	create<Endpoint_kobj>(phys_alloc, platform.core_cnode().sel(), ep_sel);

	/* allocate asynchronous object within core's CSpace */
	lock_sel = platform.core_sel_alloc().alloc();
	create<Notification_kobj>(phys_alloc, platform.core_cnode().sel(), lock_sel);

	/* assign IPC buffer to thread */
	{
		/* determine page frame selector of the allocated IPC buffer */
		Cap_sel ipc_buffer_sel = Untyped_memory::frame_sel(ipc_buffer_phys);

		int const ret = seL4_TCB_SetIPCBuffer(tcb_sel.value(), utcb_virt_addr,
		                                      ipc_buffer_sel.value());
		ASSERT(ret == 0);
	}

	/* set scheduling priority */
	enum { PRIORITY_MAX = 0xff };
	seL4_TCB_SetPriority(tcb_sel.value(), PRIORITY_MAX);
}


void Genode::Thread_info::destruct()
{
	if (lock_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, lock_sel.value(), 32);
		platform_specific()->core_sel_alloc().free(lock_sel);
	}
	if (ep_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, ep_sel.value(), 32);
		platform_specific()->core_sel_alloc().free(ep_sel);
	}
	if (tcb_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, tcb_sel.value(), 32);
		platform_specific()->core_sel_alloc().free(tcb_sel);
	}

	if (ipc_buffer_phys) {
		Platform &platform = *platform_specific();
		Range_allocator &phys_alloc = *platform.ram_alloc();
		Untyped_memory::convert_to_untyped_frames(ipc_buffer_phys, 4096);
		Untyped_memory::free_page(phys_alloc, ipc_buffer_phys);
	}
}


void Genode::start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp)
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	Genode::memset(&regs, 0, sizeof(regs));
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);

	regs.eip = ip;
	regs.esp = sp;
	regs.gs  = IPCBUF_GDT_SELECTOR;

	int const ret = seL4_TCB_WriteRegisters(tcb_sel.value(), false, 0, num_regs, &regs);
	ASSERT(ret == 0);

	seL4_TCB_Resume(tcb_sel.value());
}

#endif /* _CORE__INCLUDE__THREAD_SEL4_H_ */
