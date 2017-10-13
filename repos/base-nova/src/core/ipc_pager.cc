/*
 * \brief  Low-level page-fault handling
 * \author Norman Feske
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* Core includes */
#include <ipc_pager.h>

/* NOVA includes */
#include <nova/syscalls.h>


using namespace Genode;

Ipc_pager::Ipc_pager(Nova::Utcb * utcb, addr_t pd_dst, addr_t pd_core)
:
	_pd_dst(pd_dst),
	_pd_core(pd_core),
	_fault_ip(utcb->ip),
	_fault_addr(utcb->qual[1]),
	_sp(utcb->sp),
	_fault_type(utcb->qual[0]),
	_syscall_res(Nova::NOVA_OK),
	_normal_ipc((((addr_t)&utcb->qual[2] - (addr_t)utcb->msg()) / sizeof(addr_t))
	            == utcb->msg_words())
{

	/*
	 * When this function is called from the page-fault handler EC, a page
	 * fault already occurred. So we never wait but immediately read the
	 * page-fault information from our UTCB.
	 */
}


void Ipc_pager::set_reply_mapping(Mapping m)
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread::myself()->utcb();
	utcb->set_msg_word(0);
	bool res = utcb->append_item(m.mem_crd(), 0, true, false,
	                             false, m.dma(), m.write_combined());
	/* one item ever fits on the UTCB */
	(void)res;

	/* receive window in destination pd */
	Nova::Mem_crd crd_mem(m.dst_addr() >> 12, m.mem_crd().order(),
	                      Nova::Rights(m.mem_crd().rights().readable(),
	                                   m.mem_crd().rights().writeable(),
	                                   m.mem_crd().rights().executable()));
	/* asynchronously map memory */
	_syscall_res = Nova::delegate(_pd_core, _pd_dst, crd_mem);
}


void Ipc_pager::reply_and_wait_for_fault(addr_t sm)
{
	Thread * myself = Thread::myself();
	Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());

	utcb->mtd = 0;

	/*
	 * If it was a normal IPC and the mapping failed, caller may re-try.
	 * Otherwise nothing left to be delegated - done asynchronously beforehand.
	 */
	utcb->set_msg_word((_normal_ipc && _syscall_res != Nova::NOVA_OK) ? 1 : 0);

	Nova::reply(myself->stack_top(), sm);
}
