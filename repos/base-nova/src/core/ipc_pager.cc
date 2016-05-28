/*
 * \brief  Low-level page-fault handling
 * \author Norman Feske
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* Core includes */
#include <ipc_pager.h>

/* NOVA includes */
#include <nova/syscalls.h>


using namespace Genode;

void Ipc_pager::wait_for_fault()
{
	/*
	 * When this function is called from the page-fault handler EC, a page
	 * fault already occurred. So we never wait but immediately read the
	 * page-fault information from our UTCB.
	 */
	Nova::Utcb *utcb = (Nova::Utcb *)Thread::myself()->utcb();
	_fault_type = utcb->qual[0];
	_fault_addr = utcb->qual[1];
	_fault_ip   = utcb->ip;
}


void Ipc_pager::set_reply_mapping(Mapping m)
{
	Nova::Utcb *utcb = (Nova::Utcb *)Thread::myself()->utcb();
	utcb->set_msg_word(0);
	bool res = utcb->append_item(m.mem_crd(), m.dst_addr(), true, false,
	                             false, m.dma(), m.write_combined());
	/* one item ever fits on the UTCB */
	(void)res;
}


void Ipc_pager::reply_and_wait_for_fault(unsigned sm)
{
	Nova::reply(Thread::myself()->stack_top(), sm);
}
