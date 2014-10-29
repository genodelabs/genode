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


void second_thread_entry()
{
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

	enum { SECOND_THREAD_CAP = 0x100 };

	{
		seL4_Untyped const service     = 0x38; /* untyped */
		int          const type        = seL4_TCBObject;
		int          const offset      = 0;
		int          const size_bits   = 0;
		seL4_CNode   const root        = seL4_CapInitThreadCNode;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = SECOND_THREAD_CAP;
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

		PDBG("seL4_Untyped_RetypeAtOffset returned %d", ret);
	}

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


	*(int *)0x1122 = 0;
	return 0;
}
