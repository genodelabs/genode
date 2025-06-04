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
#include <base/internal/native_utcb.h>

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

	typedef Allocator::Alloc_result Ipc_buffer_phys;

	using Utcb_virt = Native_utcb::Virt;
}


struct Genode::Thread_info
{
	Cap_sel tcb_sel  { 0 };
	Cap_sel ep_sel   { 0 };
	Cap_sel lock_sel { 0 };
	Cap_sel vcpu_sel { 0 };

	addr_t vcpu_state_phys { 0 };

	inline void write_thread_info_to_ipc_buffer(Cap_sel pd_ep_sel);

	Allocator::Alloc_result tcb_phys  { };
	Allocator::Alloc_result ep_phys   { };
	Allocator::Alloc_result lock_phys { };
	Allocator::Alloc_result ipc_phys  { };

	Thread_info() { }

	inline bool init_tcb(Core::Platform &, Range_allocator &,
	                     unsigned const prio, unsigned const cpu,
	                     bool auto_deallocate = false);
	inline void init(Core::Utcb_virt const utcb_virt,
	                 unsigned const prio,
	                 bool auto_deallocate = true);
	inline void destruct();

	bool init_vcpu(Core::Platform &, Cap_sel ept);

	bool valid() const
	{
		return tcb_sel .value() &&  ep_sel  .value() &&
		       lock_sel.value() && !ipc_phys.failed();
	}
};


bool Genode::Thread_info::init_tcb(Core::Platform &platform,
                                   Range_allocator &phys_alloc,
                                   unsigned const prio,
                                   unsigned const cpu,
                                   bool auto_deallocate)
{
	using namespace Core;

	bool ok = false;

	/* allocate TCB within core's CNode */
	tcb_phys = Untyped_memory::alloc_page(phys_alloc);

	tcb_phys.with_result([&](auto &result) {
		seL4_Untyped const service = Untyped_memory::untyped_sel(addr_t(result.ptr)).value();

		platform.core_sel_alloc().alloc().with_result([&](auto sel) {
			auto cap_sel = Cap_sel(unsigned(sel));
			if (!create<Tcb_kobj>(service, platform.core_cnode().sel(),
			                      cap_sel)) {
				platform.core_sel_alloc().free(cap_sel);
				return;
			}

			tcb_sel = cap_sel;

			/* set scheduling priority */
			seL4_TCB_SetMCPriority(tcb_sel.value(), Cnode_index(seL4_CapInitThreadTCB).value(), prio);
			seL4_TCB_SetPriority(tcb_sel.value(), Cnode_index(seL4_CapInitThreadTCB).value(), prio);

			/* place at cpu */
			affinity_sel4_thread(tcb_sel, cpu);

			ok = true;
		}, [&](auto) { /* ok is false */ });

		result.deallocate = !ok ? true : auto_deallocate;

	}, [&](auto) {  /* ok is false */});

	return ok;
}


void Genode::Thread_info::init(Core::Utcb_virt const utcb_virt,
                               unsigned        const prio,
                               bool            const auto_deallocate)
{
	using namespace Core;

	Core::Platform  &platform   = platform_specific();
	Range_allocator &phys_alloc = platform.ram_alloc();

	ipc_phys = Untyped_memory::alloc_page(phys_alloc);

	/* create IPC buffer of one page */
	if (!ipc_phys.convert<bool>(
		[&](auto &result) {
			bool ok = Untyped_memory::convert_to_page_frames(addr_t(result.ptr), 1);
			result.deallocate = !ok ? true : auto_deallocate;
			return ok;
		}, [](auto) { return false; })) {
		ipc_phys = { };
		return;
	}

	/* allocate TCB within core's CNode */
	if (!init_tcb(platform, phys_alloc, prio, 0, auto_deallocate)) {
		tcb_phys = { };
		return;
	}

	ep_phys   = Untyped_memory::alloc_page(phys_alloc);
	lock_phys = Untyped_memory::alloc_page(phys_alloc);

	/* allocate synchronous endpoint within core's CNode */
	ep_phys.with_result([&](auto &result) {
		result.deallocate = auto_deallocate;

		auto service = Untyped_memory::untyped_sel(addr_t(result.ptr)).value();

		platform.core_sel_alloc().alloc().with_result([&](auto sel) {
			auto cap_sel = Cap_sel(unsigned(sel));
			if (!create<Endpoint_kobj>(service, platform.core_cnode().sel(),
			                           cap_sel)) {
				platform.core_sel_alloc().free(cap_sel);
				return;
			}
			ep_sel = cap_sel;
		}, [](auto) { /* ep_sel stays invalid, which is checked for */ });
	}, [](auto) { /* ep_sel stays invalid, which is checked for */ });

	/* allocate asynchronous object within core's CSpace */
	lock_phys.with_result([&](auto &result) {
		result.deallocate = auto_deallocate;

		auto service = Untyped_memory::untyped_sel(addr_t(result.ptr)).value();

		platform.core_sel_alloc().alloc().with_result([&](auto sel) {
			auto cap_sel = Cap_sel(unsigned(sel));
			if (!create<Notification_kobj>(service,
			                               platform.core_cnode().sel(),
			                               cap_sel)) {
				platform.core_sel_alloc().free(cap_sel);
				return;
			}

			lock_sel = cap_sel;
		}, [](auto) { /* lock_sel stays invalid, which is checked for */ });
	}, [](auto) { /* lock_sel stays invalid, which is checked for */ });

	/* assign IPC buffer to thread */
	ipc_phys.with_result([&](auto &result) {
		/* determine page frame selector of the allocated IPC buffer */
		Cap_sel ipc_buffer_sel = Untyped_memory::frame_sel(addr_t(result.ptr));

		int const ret = seL4_TCB_SetIPCBuffer(tcb_sel.value(), utcb_virt.addr,
		                                      ipc_buffer_sel.value());
		ASSERT(ret == 0);
	}, [](auto) { error("ipc buffer issue"); });
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
		error(__func__, " vcpu memory leakage");
		/* XXX free 16k memory */
		seL4_CNode_Delete(seL4_CapInitThreadCNode, vcpu_sel.value(), 32);
		platform_specific().core_sel_alloc().free(vcpu_sel);
	}

	ipc_phys.with_result([&](auto &result) {
		Untyped_memory::convert_to_untyped_frames(addr_t(result.ptr), 4096);
	}, [](auto) { /* in case of invalid IPC there is nothing to do */ });
}

#endif /* _CORE__INCLUDE__THREAD_SEL4_H_ */
