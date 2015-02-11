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


enum { SEL4_TCB_SIZE        = 0x1000,
       SEL4_EP_SIZE         = 16,
       SEL4_PAGE_TABLE_SIZE = 1UL << 12,
       SEL4_PAGE_SIZE       = 1UL << 12 };

/*
 * Capability for the second thread's TCB
 */
enum { SECOND_THREAD_CAP = 0x100 };

/*
 * Capability for IPC entrypoint, set up by the main thread, used by the second
 * thread.
 */
enum { EP_CAP = 0x101 };

/*
 * Capability slot used by the second thread to receive a capability via IPC.
 */
enum { RECV_CAP = 0x102 };

/*
 * Minted endpoint capability, derived from EP_CAP
 */
enum { EP_MINTED_CAP = 0x103 };

/*
 * Capabilities for a custom created page table, a page, and a second cap
 * for the same page.
 */
enum { PAGE_TABLE_CAP = 0x104, PAGE_CAP = 0x105, PAGE_CAP_2 = 0x106 };


void second_thread_entry()
{
	init_ipc_buffer();

	for (;;) {
		seL4_SetCapReceivePath(seL4_CapInitThreadCNode, RECV_CAP, 32);

		seL4_CNode_Delete(seL4_CapInitThreadCNode, RECV_CAP, 32);


		PDBG("call seL4_Wait");
		seL4_MessageInfo_t msg_info = seL4_Wait(EP_CAP, nullptr);
		PDBG("returned from seL4_Wait, call seL4_Reply");

		PDBG("msg_info: got unwrapped  %d", seL4_MessageInfo_get_capsUnwrapped(msg_info));
		PDBG("          got extra caps %d", seL4_MessageInfo_get_extraCaps(msg_info));
		PDBG("          label          %d", seL4_MessageInfo_get_label(msg_info));

		if (seL4_MessageInfo_get_capsUnwrapped(msg_info)) {
			PDBG("          badge          %d", seL4_CapData_Badge_get_Badge(seL4_GetBadge(0)));
		}

		seL4_Reply(msg_info);
		PDBG("returned from seL4_Reply");
	}
}


/**
 * Return cap selector of largest available untyped memory range
 */
static seL4_Untyped const largest_untyped_range(seL4_BootInfo const &bi)
{
	using Genode::size_t;

	seL4_Untyped largest = 0;
	size_t largest_size = 0;

	unsigned const idx_start = bi.untyped.start;
	unsigned const idx_size  = bi.untyped.end - idx_start;

	for (unsigned i = idx_start; i < idx_start + idx_size; i++) {

		size_t const size = (1UL << bi.untypedSizeBitsList[i - idx_start]) - 1;

		if (size <= largest_size)
			continue;

		largest_size = size;
		largest      = i;
	}

	return largest;
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

	/* yse largest untyped memory region for allocating kernel objects */
	seL4_Untyped const untyped = largest_untyped_range(*bi);

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

	PDBG("seL4_Call, delegating a TCB capability");
	{
		seL4_MessageInfo_t msg_info = seL4_MessageInfo_new(13, 0, 1, 0);

		seL4_SetCap(0, SECOND_THREAD_CAP);

		seL4_Call(EP_CAP, msg_info);

		PDBG("returned from seL4_Call");
	}

	PDBG("seL4_Call, delegating a TCB capability");
	{
		seL4_MessageInfo_t msg_info = seL4_MessageInfo_new(13, 0, 1, 0);

		seL4_SetCap(0, SECOND_THREAD_CAP);

		seL4_Call(EP_CAP, msg_info);

		PDBG("returned from seL4_Call");
	}

	PDBG("seL4_Call, delegating a minted endpoint capability");
	{
		/* mint EP_CAP into EP_MINTED_CAP */
		seL4_CNode     const service    = seL4_CapInitThreadCNode;
		seL4_Word      const dest_index = EP_MINTED_CAP;
		uint8_t        const dest_depth = 32;
		seL4_CNode     const src_root   = seL4_CapInitThreadCNode;
		seL4_Word      const src_index  = EP_CAP;
		uint8_t        const src_depth  = 32;
		seL4_CapRights const rights     = seL4_Transfer_Mint;
		seL4_CapData_t const badge      = seL4_CapData_Badge_new(111);

		int const ret = seL4_CNode_Mint(service,
		                                dest_index,
		                                dest_depth,
		                                src_root,
		                                src_index,
		                                src_depth,
		                                rights,
		                                badge);

		PDBG("seL4_CNode_Mint (EP_MINTED_CAP) returned %d", ret);

		seL4_MessageInfo_t msg_info = seL4_MessageInfo_new(13, 0, 3, 0);

		/*
		 * Supply 3 capabilities as arguments. The first and third will be
		 * unwrapped at the receiver, the second will be delegated.
		 */
		seL4_SetCap(0, EP_MINTED_CAP);
		seL4_SetCap(1, SECOND_THREAD_CAP);
		seL4_SetCap(2, EP_MINTED_CAP);
		seL4_Call(EP_CAP, msg_info);

		PDBG("returned from seL4_Call");
	}

	/*
	 * Test the mapping of memory
	 */

	/* create page table */
	{
		/* align allocation offset */
		untyped_offset = Genode::align_addr(untyped_offset, 12);

		seL4_Untyped const service     = untyped;
		int          const type        = seL4_IA32_PageTableObject;
		int          const offset      = untyped_offset;
		int          const size_bits   = 0;
		seL4_CNode   const root        = seL4_CapInitThreadCNode;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = PAGE_TABLE_CAP;
		int          const num_objects = 1;

		untyped_offset += SEL4_PAGE_TABLE_SIZE;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		PDBG("seL4_Untyped_RetypeAtOffset (PAGE_TABLE) returned %d", ret);
	}

	/* create 4K page */
	{
		/* align allocation offset */
		untyped_offset = Genode::align_addr(untyped_offset, 12);

		seL4_Untyped const service     = untyped;
		int          const type        = seL4_IA32_4K;
		int          const offset      = untyped_offset;
		int          const size_bits   = 0;
		seL4_CNode   const root        = seL4_CapInitThreadCNode;
		int          const node_index  = 0;
		int          const node_depth  = 0;
		int          const node_offset = PAGE_CAP;
		int          const num_objects = 1;

		untyped_offset += SEL4_PAGE_SIZE;

		int const ret = seL4_Untyped_RetypeAtOffset(service,
		                                            type,
		                                            offset,
		                                            size_bits,
		                                            root,
		                                            node_index,
		                                            node_depth,
		                                            node_offset,
		                                            num_objects);

		PDBG("seL4_Untyped_RetypeAtOffset (PAGE) returned %d", ret);
	}

	/* add page table into our page directory at address 0x40000000 */
	{
		seL4_IA32_PageTable     const service = PAGE_TABLE_CAP;
		seL4_IA32_PageDirectory const pd      = seL4_CapInitThreadPD;
		seL4_Word               const vaddr   = 0x40000000;
		seL4_IA32_VMAttributes  const attr    = seL4_IA32_Default_VMAttributes;

		int const ret = seL4_IA32_PageTable_Map(service, pd, vaddr, attr);

		PDBG("seL4_IA32_PageTable_Map returned %d", ret);
	}

	/* add page to page table at 0x40001000 */
	{
		seL4_IA32_Page          const service = PAGE_CAP;
		seL4_IA32_PageDirectory const pd      = seL4_CapInitThreadPD;
		seL4_Word               const vaddr   = 0x40001000;
		seL4_CapRights          const rights  = seL4_AllRights;
		seL4_IA32_VMAttributes  const attr    = seL4_IA32_Default_VMAttributes;

		int const ret = seL4_IA32_Page_Map(service, pd, vaddr, rights, attr);

		PDBG("seL4_IA32_Page_Map to 0x%lx returned %d", (unsigned long)vaddr, ret);
	}

	/*
	 * We cannot use the same PAGE_CAP for the seoncd mapping (see Chapter
	 * 6.4 of the seL4 manual). So we need to create and use a copy of the
	 * page cap.
	 */

	{
		seL4_CNode     const service    =  seL4_CapInitThreadCNode;
		seL4_Word      const dest_index = PAGE_CAP_2;
		uint8_t        const dest_depth = 32;
		seL4_CNode     const src_root   = seL4_CapInitThreadCNode;
		seL4_Word      const src_index  = PAGE_CAP;
		uint8_t        const src_depth  = 32;
		seL4_CapRights const rights     = seL4_AllRights;

		int const ret = seL4_CNode_Copy(service, dest_index, dest_depth,
		                                src_root, src_index, src_depth, rights);

		PDBG("seL4_CNode_Copy returned %d", ret);
	}

	/* add the same page to page table at 0x40002000 */
	{
		seL4_IA32_Page          const service = PAGE_CAP_2;
		seL4_IA32_PageDirectory const pd      = seL4_CapInitThreadPD;
		seL4_Word               const vaddr   = 0x40002000;
		seL4_CapRights          const rights  = seL4_AllRights;
		seL4_IA32_VMAttributes  const attr    = seL4_IA32_Default_VMAttributes;

		int const ret = seL4_IA32_Page_Map(service, pd, vaddr, rights, attr);

		PDBG("seL4_IA32_Page_Map to 0x%lx returned %d", (unsigned long)vaddr, ret);
	}

	/* write data to the first mapping of the page */
	Genode::strncpy((char *)0x40001000, "Data written to 0x40001000", 100);

	/* print content of the second mapping */
	PLOG("read from 0x40002000: \"%s\"", (char const *)0x40002000);

	*(int *)0x1122 = 0;
	return 0;
}
