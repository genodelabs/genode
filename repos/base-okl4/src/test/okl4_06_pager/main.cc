/*
 * \brief  Test for creating and paging address spaces
 * \author Norman Feske
 * \date   2009-03-28
 *
 * This program can be started as roottask replacement directly on the
 * OKL4 kernel.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

namespace Okl4 { extern "C" {
#include <l4/space.h>
} }

/* local includes */
#include "../mini_env.h"
#include "../create_thread.h"

using namespace Genode;
using namespace Okl4;


/**
 * Entry of child address space
 */
static void subspace_thread_entry()
{
	static char read_area[4096*2];
	static char write_area[4096*2];
	thread_init_myself();

	int a = 0;

	for (unsigned i = 0; i < sizeof(read_area); i++)
		a += read_area[i];

	for (unsigned i = 0; i < sizeof(write_area); i++)
		write_area[i] = a;

	for (;;) L4_Yield();
}


/**
 * Print page-fault information in a human-readable form
 */
static inline void print_page_fault(L4_Word_t type, L4_Word_t addr, L4_Word_t ip)
{
	printf("page (%s%s%s) fault at pf_addr=%lx, pf_ip=%lx\n",
	       type & L4_Readable   ? "r" : "-",
	       type & L4_Writable   ? "w" : "-",
	       type & L4_eXecutable ? "x" : "-",
	       addr, ip);
}


/**
 * Main program
 */
int main()
{
	roottask_init_myself();

	/* set default priority for ourself to make round-robin scheduling work */
	L4_Set_Priority(L4_Myself(), DEFAULT_PRIORITY);

	enum { NEW_SPACE_ID       =  1 };
	enum { FPAGE_LOG2_SIZE_4K = 12 };

	/* create address space */
	L4_SpaceId_t space      = L4_SpaceId(NEW_SPACE_ID);
	L4_Word_t    control    = L4_SpaceCtrl_new;
	L4_ClistId_t cap_list   = L4_rootclist;
	L4_Fpage_t   utcb_area  = L4_FpageLog2((L4_Word_t)utcb_base_get() +
	                                       NEW_SPACE_ID*L4_GetUtcbAreaSize(),
	                                       FPAGE_LOG2_SIZE_4K);
#ifdef NO_UTCB_RELOCATE
	utcb_area = L4_Nilpage;  /* UTCB allocation is handled by the kernel */
#endif

	L4_Word_t resources     = 0;
	L4_Word_t old_resources = 0;

	int ret = L4_SpaceControl(space, control, cap_list, utcb_area,
	                          resources, &old_resources);

	if (ret != 1)
		PERR("L4_SpaceControl returned %d, error code=%d",
		     ret, (int)L4_ErrorCode());

	/* create main thread for new address space */
	enum { THREAD_STACK_SIZE = 4096 };
	static int thread_stack[THREAD_STACK_SIZE];
	create_thread(1, NEW_SPACE_ID,
	              (void *)(&thread_stack[THREAD_STACK_SIZE]),
	              subspace_thread_entry);

	printf("entering pager loop\n");

	for (;;) {
		L4_ThreadId_t faulter;

		/* wait for page fault */
		L4_MsgTag_t faulter_tag = L4_Wait(&faulter);

		/* read fault information */
		L4_Word_t     pf_type, pf_ip, pf_addr;
		L4_StoreMR(1, &pf_addr);
		L4_StoreMR(2, &pf_ip);
		pf_type = L4_Label(faulter_tag) & 7;

		print_page_fault(pf_type, pf_addr, pf_ip);

		/* determine corresponding page in our own address space */
		pf_addr &= ~(4096 - 1);
		L4_Fpage_t fpage = L4_FpageLog2(pf_addr, 12);
		fpage.X.rwx = 7;

		/* request physical address of page */
		L4_MapItem_t  map_item;
		L4_PhysDesc_t phys_desc;
		L4_ReadFpage(L4_SpaceId(0), fpage, &phys_desc, &map_item);

		/* map page to faulting space */
		int ret = L4_MapFpage(L4_SenderSpace(), fpage, phys_desc);

		if (ret != 1)
			PERR("L4_MapFpage returned %d, error_code=%d",
			     ret, (int)L4_ErrorCode());

		/* reply to page-fault message to resume the faulting thread */
		L4_LoadMR(0, 0);
		L4_Send(faulter);
	}
	return 0;
}
