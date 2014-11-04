/*
 * \brief  Test for exercising the seL4 kernel interface
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/stdint.h>
#include <util/string.h>

/* seL4 includes */
#include <sel4/bootinfo.h>
#include <sel4/interfaces/sel4_client.h>


static seL4_BootInfo const *boot_info()
{
	extern Genode::addr_t __initial_bx;
	return (seL4_BootInfo const *)__initial_bx;
}


static void dump_untyped_ranges(seL4_BootInfo const &bi,
                                unsigned start, unsigned size)
{
	for (unsigned i = start; i < start + size; i++) {

		/* index into 'untypedPaddrList' */
		unsigned const k = i - bi.untyped.start;

		PINF("                         [%02x] [%08lx,%08lx]",
		     i,
		     (long)bi.untypedPaddrList[k],
		     (long)bi.untypedPaddrList[k] + (1UL << bi.untypedSizeBitsList[k]) - 1);
	}
}


static void dump_boot_info(seL4_BootInfo const &bi)
{
	PINF("--- boot info at %p ---", &bi);
	PINF("ipcBuffer:               %p", bi.ipcBuffer);
	PINF("initThreadCNodeSizeBits: %d", (int)bi.initThreadCNodeSizeBits);
	PINF("untyped:                 [%x,%x)", bi.untyped.start, bi.untyped.end);

	dump_untyped_ranges(bi, bi.untyped.start,
	                    bi.untyped.end - bi.untyped.start);

	PINF("deviceUntyped:           [%x,%x)",
	     bi.deviceUntyped.start, bi.deviceUntyped.end);

	dump_untyped_ranges(bi, bi.deviceUntyped.start,
	                    bi.deviceUntyped.end - bi.deviceUntyped.start);
}


static inline void init_ipc_buffer()
{
	asm volatile ("movl %0, %%gs" :: "r"(IPCBUF_GDT_SELECTOR) : "memory");
}


enum { SEL4_TCB_SIZE = 0x1000,
       SEL4_EP_SIZE  = 16 };

/*
 * Capability for the second thread's TCB
 */
enum { SECOND_THREAD_CAP = 0x100 };

/*
 * Capability for IPC entrypoint, set up by the main thread, used by the second
 * thread.
 */
enum { EP_CAP = 0x101 };


void second_thread_entry()
{
	init_ipc_buffer();

	PDBG("call seL4_Wait");
	seL4_MessageInfo_t msg_info = seL4_Wait(EP_CAP, nullptr);
	PDBG("returned from seL4_Wait, call seL4_Reply");
	seL4_Reply(msg_info);
	PDBG("returned from seL4_Reply");

	*(int *)0x2244 = 0;
}


extern char _bss_start, _bss_end;

int main()
{
	seL4_BootInfo const *bi = boot_info();

	dump_boot_info(*bi);

	PDBG("set_ipc_buffer");

	init_ipc_buffer();

	PDBG("seL4_SetUserData");
	seL4_SetUserData((seL4_Word)bi->ipcBuffer);

	/* yse first untyped memory region for allocating kernel objects */
	seL4_Untyped const untyped = bi->untyped.start;

	/* offset to next free position within the untyped memory range */
	unsigned long untyped_offset = 0;

	/* create second thread */
	{
		seL4_Untyped const service     = untyped;
		int          const type        = seL4_TCBObject;
		int          const offset      = untyped_offset;
		int          const size_bits   = 0;
		seL4_CNode   const root        = seL4_CapInitThreadCNode;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = SECOND_THREAD_CAP;
		int          const num_objects = 1;

		untyped_offset += SEL4_TCB_SIZE;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		PDBG("seL4_Untyped_RetypeAtOffset (TCB) returned %d", ret);
	}

	/* create synchronous IPC entrypoint */
	{
		seL4_Untyped const service     = untyped;
		int          const type        = seL4_EndpointObject;
		int          const offset      = untyped_offset;
		int          const size_bits   = 0;
		seL4_CNode   const root        = seL4_CapInitThreadCNode;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = EP_CAP;
		int          const num_objects = 1;

		untyped_offset += SEL4_EP_SIZE;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		PDBG("seL4_Untyped_RetypeAtOffset (EP) returned %d", ret);
	}

	/* assign IPC buffer to second thread */
	{
		static_assert(sizeof(seL4_IPCBuffer) % 512 == 0,
		              "unexpected seL4_IPCBuffer size");

		int const ret = seL4_TCB_SetIPCBuffer(SECOND_THREAD_CAP,
		                                      (seL4_Word)(bi->ipcBuffer + 1),
		                                      seL4_CapInitThreadIPCBuffer);

		PDBG("seL4_TCB_SetIPCBuffer returned %d", ret);
	}

	/* start second thread */
	long stack[0x1000];
	{
		seL4_UserContext regs;
		Genode::memset(&regs, 0, sizeof(regs));

		regs.eip = (uint32_t)&second_thread_entry;
		regs.esp = (uint32_t)&stack[0] + sizeof(stack);
		int const ret = seL4_TCB_WriteRegisters(SECOND_THREAD_CAP, false,
		                                        0, 2, &regs);
		PDBG("seL4_TCB_WriteRegisters returned %d", ret);
	}

	{
		seL4_CapData_t no_cap_data = { { 0 } };
		int const ret = seL4_TCB_SetSpace(SECOND_THREAD_CAP, 0,
		                  seL4_CapInitThreadCNode, no_cap_data,
		                  seL4_CapInitThreadPD, no_cap_data);
		PDBG("seL4_TCB_SetSpace returned %d", ret);
	}

	seL4_TCB_Resume(SECOND_THREAD_CAP);

	seL4_TCB_SetPriority(SECOND_THREAD_CAP, 0xff);

	PDBG("call seL4_Call");

	seL4_MessageInfo_t msg_info = seL4_MessageInfo_new(0, 0, 0, 0);
	seL4_Call(EP_CAP, msg_info);

	PDBG("returned from seL4_Call");

	*(int *)0x1122 = 0;
	return 0;
}
