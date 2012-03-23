/*
 * \brief  Pager support for OKL4
 * \author Norman Feske
 * \date   2009-03-31
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/ipc_pager.h>
#include <base/printf.h>

namespace Okl4 { extern "C" {
#include <l4/message.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
} }

static const bool verbose_page_fault = false;
static const bool verbose_exception  = false;


using namespace Genode;
using namespace Okl4;

/**
 * Print page-fault information in a human-readable form
 */
static inline void print_page_fault(L4_Word_t type, L4_Word_t addr, L4_Word_t ip,
                                    unsigned long badge)
{
	printf("page (%s%s%s) fault at fault_addr=%lx, fault_ip=%lx, from=%lx\n",
	       type & L4_Readable   ? "r" : "-",
	       type & L4_Writable   ? "w" : "-",
	       type & L4_eXecutable ? "x" : "-",
	       addr, ip, badge);
}


/**
 * Return the global thread ID of the calling thread
 *
 * On OKL4 we cannot use 'L4_Myself()' to determine our own thread's
 * identity. By convention, each thread stores its global ID in a
 * defined entry of its UTCB.
 */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}


/*************
 ** Mapping **
 *************/

Mapping::Mapping(addr_t dst_addr, addr_t src_addr,
                 bool write_combined, unsigned l2size, bool rw)
:
	_fpage(L4_FpageLog2(dst_addr, l2size)),
	/*
	 * OKL4 does not support write-combining as mapping attribute.
	 */
	_phys_desc(L4_PhysDesc(src_addr, 0))
{
	L4_Set_Rights(&_fpage, rw ? L4_ReadWriteOnly : L4_ReadeXecOnly);
}


Mapping::Mapping() { }


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	/* wait for fault */
	_faulter_tag = L4_Wait(&_last);

	/*
	 * Read fault information
	 */

	/* exception */
	if (is_exception()) {
		L4_StoreMR(1, &_fault_ip);

		if (verbose_exception)
			PERR("Exception (label 0x%x) occured in space %d at IP 0x%p",
			     (int)L4_Label(_faulter_tag), (int)L4_SenderSpace().raw,
			     (void *)_fault_ip);
	}

	/* page fault */
	else {
		L4_StoreMR(1, &_fault_addr);
		L4_StoreMR(2, &_fault_ip);

		if (verbose_page_fault)
			print_page_fault(L4_Label(_faulter_tag), _fault_addr, _fault_ip, _last.raw);
	}
	_last_space = L4_SenderSpace().raw;
}


void Ipc_pager::reply_and_wait_for_fault()
{
	L4_SpaceId_t to_space;
	to_space.raw = L4_ThreadNo(_last) >> Thread_id_bits::THREAD;

	/* map page to faulting space */
	int ret = L4_MapFpage(to_space, _reply_mapping.fpage(),
	                                _reply_mapping.phys_desc());

	if (ret != 1)
		PERR("L4_MapFpage returned %d, error_code=%d",
		     ret, (int)L4_ErrorCode());

	/* reply to page-fault message to resume the faulting thread */
	acknowledge_wakeup();

	wait_for_fault();
}


void Ipc_pager::acknowledge_wakeup()
{
	/* answer wakeup call from one of core's region-manager sessions */
	L4_LoadMR(0, 0);
	L4_Send(_last);
}


Ipc_pager::Ipc_pager() : Native_capability(thread_get_my_global_id(), 0) { }

