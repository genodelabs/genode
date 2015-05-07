/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>
#include <untyped_memory.h>
#include <map_local.h>

using namespace Genode;


static Untyped_address create_and_map_ipc_buffer(Range_allocator &phys_alloc,
                                                 addr_t virt_addr)
{
	/* create IPC buffer of one page */
	size_t const ipc_buffer_size_log2 = get_page_size_log2();
	Untyped_address const untyped_addr =
		Untyped_memory::alloc_log2(phys_alloc, ipc_buffer_size_log2);

	Untyped_memory::convert_to_page_frames(untyped_addr.phys(), 1);

	if (!map_local(untyped_addr.phys(), virt_addr, 1)) {
		PERR("could not map IPC buffer phys %lx at local %lx",
		     untyped_addr.phys(), virt_addr);
	}

	return untyped_addr;
}


static void create_tcb(Cnode &core_cnode, Range_allocator &phys_alloc,
                       unsigned dst_idx)
{
	/* create TCB */
	size_t const tcb_size_log2 = get_page_size_log2();
	Untyped_address const untyped_addr =
		Untyped_memory::alloc_log2(phys_alloc, tcb_size_log2);

	seL4_Untyped const service     = untyped_addr.sel();
	int          const type        = seL4_TCBObject;
	int          const offset      = untyped_addr.offset();
	int          const size_bits   = 0;
	seL4_CNode   const root        = core_cnode.sel();
	int          const node_index  = 0;
	int          const node_depth  = 0;
	int          const node_offset = dst_idx;
	int          const num_objects = 1;

	int const ret = seL4_Untyped_RetypeAtOffset(service,
	                                            type,
	                                            offset,
	                                            size_bits,
	                                            root,
	                                            node_index,
	                                            node_depth,
	                                            node_offset,
	                                            num_objects);

	if (ret != 0)
		PDBG("seL4_Untyped_RetypeAtOffset (TCB) returned %d", ret);
}


void Thread_base::_init_platform_thread(size_t, Type type)
{
	Platform &platform = *platform_specific();
	Range_allocator &phys_alloc = *platform.ram_alloc();

	addr_t const utcb_virt_addr = (addr_t)&_context->utcb;
	Untyped_address const ipc_buffer =
		create_and_map_ipc_buffer(phys_alloc, utcb_virt_addr);

	/* allocate TCB selector within core's CNode */
	unsigned const tcb_idx = platform.alloc_core_sel();

	create_tcb(platform.core_cnode(), phys_alloc, tcb_idx);
	unsigned const tcb_sel = tcb_idx;
	_tid.tcb_sel = tcb_sel;

	/* assign IPC buffer to thread */
	{
		/* determine page frame selector of the allocated IPC buffer */
		unsigned ipc_buffer_sel = Untyped_memory::frame_sel(ipc_buffer.phys());

		int const ret = seL4_TCB_SetIPCBuffer(tcb_sel, utcb_virt_addr, ipc_buffer_sel);
		if (ret != 0)
			PDBG("seL4_TCB_SetIPCBuffer returned %d", ret);
	}

	/* set scheduling priority */
	enum { PRIORITY_MAX = 0xff };
	seL4_TCB_SetPriority(tcb_sel, PRIORITY_MAX);

	/* associate thread with core PD */
	{
		seL4_CapData_t no_cap_data = { { 0 } };
		int const ret = seL4_TCB_SetSpace(tcb_sel, 0,
		                  platform.top_cnode().sel(), no_cap_data,
		                  seL4_CapInitThreadPD, no_cap_data);

		if (ret != 0)
			PDBG("seL4_TCB_SetSpace returned %d", ret);
	}
}


void Thread_base::_deinit_platform_thread()
{
	PDBG("not implemented");
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();

	sleep_forever();
}


void Thread_base::start()
{
	/* set register values for the instruction pointer and stack pointer */
	seL4_UserContext regs;
	Genode::memset(&regs, 0, sizeof(regs));

	regs.eip = (uint32_t)&_thread_start;
	regs.esp = (uint32_t)stack_top();
	regs.gs  = IPCBUF_GDT_SELECTOR;
	size_t const num_regs = sizeof(regs)/sizeof(seL4_Word);
	int const ret = seL4_TCB_WriteRegisters(_tid.tcb_sel, false,
	                                        0, num_regs, &regs);

	if (ret != 0)
		PDBG("seL4_TCB_WriteRegisters returned %d", ret);

	seL4_TCB_Resume(_tid.tcb_sel);
}


void Thread_base::cancel_blocking()
{
	PWRN("not implemented");
}

