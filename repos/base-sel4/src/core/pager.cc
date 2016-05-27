/*
 * \brief  Pager
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
#include <base/sleep.h>

/* core includes */
#include <pager.h>
#include <core_capability_space.h>
#include <install_mapping.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

/* seL4 includes */
#include <sel4/sel4.h>
#include <sel4/arch/pfIPC.h>

using namespace Genode;

static bool const verbose = false;


struct Fault_info
{
	addr_t ip    = 0;
	addr_t pf    = 0;
	bool   write = 0;

	Fault_info(seL4_MessageInfo_t msg_info)
	:
		ip(seL4_GetMR(0)),
		pf(seL4_GetMR(1)),
		write(seL4_Fault_isWriteFault(seL4_GetMR(3)))
	{
		if (verbose)
			PINF("PF: ip=0x%lx, pf=0x%lx, write=%d", ip, pf, write);
	}
};


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	_last = 0;
	reply_and_wait_for_fault();
}


void Ipc_pager::reply_and_wait_for_fault()
{
	if (_last)
		install_mapping(_reply_mapping, _last);

	seL4_Word badge = Rpc_obj_key::INVALID;

	seL4_MessageInfo_t page_fault_msg_info;

	if (_last) {

		seL4_MessageInfo_t const reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);

		page_fault_msg_info =
			seL4_ReplyRecv(Thread::myself()->native_thread().ep_sel, reply_msg, &badge);

	} else {
		page_fault_msg_info =
			seL4_Recv(Thread::myself()->native_thread().ep_sel, &badge);
	}

	Fault_info const fault_info(page_fault_msg_info);

	_pf_ip    = fault_info.ip;
	_pf_addr  = fault_info.pf;
	_pf_write = fault_info.write;

	_last = badge;
}


void Ipc_pager::acknowledge_wakeup()
{
	PDBG("not implemented");
}


Ipc_pager::Ipc_pager() : _last(0) { }


/******************
 ** Pager object **
 ******************/

void Pager_object::wake_up()
{
	PDBG("not implemented");
}


void Pager_object::unresolved_page_fault_occurred()
{
	state.unresolved_page_fault = true;
}


/**********************
 ** Pager entrypoint **
 **********************/

Untyped_capability Pager_entrypoint::_pager_object_cap(unsigned long badge)
{
	/*
	 * Create minted endpoint capability of the pager entrypoint.
	 * The badge of the page-fault message is used to find the pager
	 * object for faulted thread.
	 */
	Rpc_obj_key rpc_obj_key((addr_t)badge);

	Untyped_capability ep_cap(Capability_space::create_ep_cap(*this));
	return Capability_space::create_rpc_obj_cap(ep_cap, nullptr, rpc_obj_key);
}

