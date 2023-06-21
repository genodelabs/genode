/*
 * \brief  Utilities for thread creation on seL4
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

namespace Genode { struct Thread_info; }

namespace Core {

	/**
	 * Set register values for the instruction pointer and stack pointer and
	 * start the seL4 thread
	 */
	void start_sel4_thread(Cap_sel tcb_sel, addr_t ip, addr_t sp, unsigned cpu,
	                       addr_t tls_ipcbuffer);
	void affinity_sel4_thread(Cap_sel const &tcb_sel, unsigned cpu);
}


struct Genode::Thread_info
{
	Cap_sel tcb_sel { 0 };
	Cap_sel ep_sel  { 0 };
	Cap_sel lock_sel { 0 };
	Cap_sel vcpu_sel { 0 };

	addr_t ipc_buffer_phys { 0 };
	addr_t vcpu_state_phys { 0 };

	inline void write_thread_info_to_ipc_buffer(Cap_sel pd_ep_sel);

	Thread_info() { }

	inline void init_tcb(Core::Platform &, Range_allocator &,
	                     unsigned const prio, unsigned const cpu);
	inline void init(addr_t const utcb_virt_addr, unsigned const prio);
	inline void destruct();

	bool init_vcpu(Core::Platform &, Cap_sel ept);
};


void Genode::Thread_info::init_tcb(Core::Platform &platform,
                                   Range_allocator &phys_alloc,
                                   unsigned const prio, unsigned const cpu)
{
	using namespace Core;

	/* allocate TCB within core's CNode */
	addr_t       const phys_addr = Untyped_memory::alloc_page(phys_alloc);
	seL4_Untyped const service   = Untyped_memory::untyped_sel(phys_addr).value();

	tcb_sel = platform.core_sel_alloc().alloc();
	create<Tcb_kobj>(service, platform.core_cnode().sel(), tcb_sel);

	/* set scheduling priority */
	seL4_TCB_SetMCPriority(tcb_sel.value(), Cnode_index(seL4_CapInitThreadTCB).value(), prio);
	seL4_TCB_SetPriority(tcb_sel.value(), Cnode_index(seL4_CapInitThreadTCB).value(), prio);

	/* place at cpu */
	affinity_sel4_thread(tcb_sel, cpu);
}


void Genode::Thread_info::init(addr_t const utcb_virt_addr, unsigned const prio)
{
	using namespace Core;

	Core::Platform  &platform   = platform_specific();
	Range_allocator &phys_alloc = platform.ram_alloc();

	/* create IPC buffer of one page */
	ipc_buffer_phys = Untyped_memory::alloc_page(phys_alloc);
	Untyped_memory::convert_to_page_frames(ipc_buffer_phys, 1);

	/* allocate TCB within core's CNode */
	init_tcb(platform, phys_alloc, prio, 0);

	/* allocate synchronous endpoint within core's CNode */
	{
		addr_t       const phys_addr = Untyped_memory::alloc_page(phys_alloc);
		seL4_Untyped const service   = Untyped_memory::untyped_sel(phys_addr).value();

		ep_sel = platform.core_sel_alloc().alloc();
		create<Endpoint_kobj>(service, platform.core_cnode().sel(), ep_sel);
	}

	/* allocate asynchronous object within core's CSpace */
	{
		addr_t       const phys_addr = Untyped_memory::alloc_page(phys_alloc);
		seL4_Untyped const service   = Untyped_memory::untyped_sel(phys_addr).value();

		lock_sel = platform.core_sel_alloc().alloc();
		create<Notification_kobj>(service, platform.core_cnode().sel(), lock_sel);
	}

	/* assign IPC buffer to thread */
	{
		/* determine page frame selector of the allocated IPC buffer */
		Cap_sel ipc_buffer_sel = Untyped_memory::frame_sel(ipc_buffer_phys);

		int const ret = seL4_TCB_SetIPCBuffer(tcb_sel.value(), utcb_virt_addr,
		                                      ipc_buffer_sel.value());
		ASSERT(ret == 0);
	}
}


void Genode::Thread_info::destruct()
{
	using namespace Core;

	if (lock_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, lock_sel.value(), 32);
		platform_specific().core_sel_alloc().free(lock_sel);
	}
	if (ep_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, ep_sel.value(), 32);
		platform_specific().core_sel_alloc().free(ep_sel);
	}
	if (tcb_sel.value()) {
		seL4_CNode_Delete(seL4_CapInitThreadCNode, tcb_sel.value(), 32);
		platform_specific().core_sel_alloc().free(tcb_sel);
	}
	if (vcpu_sel.value()) {
		/* XXX free 16k memory */
		seL4_CNode_Delete(seL4_CapInitThreadCNode, vcpu_sel.value(), 32);
		platform_specific().core_sel_alloc().free(vcpu_sel);
	}

	if (ipc_buffer_phys) {
		Core::Platform  &platform   = platform_specific();
		Range_allocator &phys_alloc = platform.ram_alloc();
		Untyped_memory::convert_to_untyped_frames(ipc_buffer_phys, 4096);
		Untyped_memory::free_page(phys_alloc, ipc_buffer_phys);
	}
}

#endif /* _CORE__INCLUDE__THREAD_SEL4_H_ */
