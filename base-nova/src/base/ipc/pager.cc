/*
 * \brief  Low-level page-fault handling
 * \author Norman Feske
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>
#include <base/thread.h>

/* NOVA includes */
#include <nova/syscalls.h>

enum { verbose_page_fault = false };

using namespace Genode;

/**
 * Print page-fault information in a human-readable form
 */
inline void print_page_fault(unsigned type, addr_t addr, addr_t ip)
{
	enum { TYPE_READ  = 0x4, TYPE_WRITE = 0x2, TYPE_EXEC  = 0x1, };
	printf("page (%s%s%s) fault at fault_addr=%lx, fault_ip=%lx\n",
	       type & TYPE_READ  ? "r" : "-",
	       type & TYPE_WRITE ? "w" : "-",
	       type & TYPE_EXEC  ? "x" : "-",
	       addr, ip);
}


void Ipc_pager::wait_for_fault()
{
	/*
	 * When this function is called from the page-fault handler EC, a page
	 * fault already occurred. So we never wait but immediately read the
	 * page-fault information from our UTCB.
	 */
	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();
	_fault_type = (Pf_type)utcb->qual[0];
	_fault_addr = utcb->qual[1];
	_fault_ip   = utcb->ip;

	if (verbose_page_fault)
		print_page_fault(_fault_type, _fault_addr, _fault_ip);
}


void Ipc_pager::set_reply_mapping(Mapping m)
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread_base::myself()->utcb();
	utcb->set_msg_word(0);
	bool res = utcb->append_item(m.mem_crd(), m.dst_addr());
	/* one item ever fits on the UTCB */
	(void)res;
}


void Ipc_pager::reply_and_wait_for_fault()
{
	Nova::reply(Thread_base::myself()->stack_top());
}
