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
#include <internal/capability_space_sel4.h>

/* seL4 includes */
#include <sel4/interfaces/sel4_client.h>
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
			seL4_ReplyWait(Thread_base::myself()->tid().ep_sel, reply_msg, &badge);

	} else {
		page_fault_msg_info =
			seL4_Wait(Thread_base::myself()->tid().ep_sel, &badge);
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


Ipc_pager::Ipc_pager()
:
	Native_capability(Capability_space::create_ep_cap(*Thread_base::myself())),
	_last(0)
{ }



/**********************
 ** Pager activation **
 **********************/

void Pager_activation_base::entry()
{
	Ipc_pager pager;
	_cap = pager;
	_cap_valid.unlock();

	bool reply_pending = false;
	while (1) {

		if (reply_pending)
			pager.reply_and_wait_for_fault();
		else
			pager.wait_for_fault();

		reply_pending = false;

		/* lookup referenced object */
		Object_pool<Pager_object>::Guard _obj(_ep ? _ep->lookup_and_lock(pager.badge()) : 0);
		Pager_object *obj = _obj;

		/* handle request */
		if (obj) {
			if (pager.is_exception()) {
				obj->submit_exception_signal();
				continue;
			}

			/* send reply if page-fault handling succeeded */
			if (!obj->pager(pager))
				reply_pending = true;
		}
	}
}


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

Pager_entrypoint::Pager_entrypoint(Cap_session *, Pager_activation_base *a)
: _activation(a)
{ _activation->ep(this); }


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	remove_locked(obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* return invalid capability if no activation is present */
	if (!_activation) return Pager_capability();

	/*
	 * Create minted endpoint capability of the pager entrypoint.
	 * The badge of the page-fault message is used to find the pager
	 * object for faulted thread.
	 */
	Native_capability ep_cap = _activation->cap();

	Rpc_obj_key rpc_obj_key((addr_t)obj->badge());

	Untyped_capability new_obj_cap =
		Capability_space::create_rpc_obj_cap(ep_cap, 0, rpc_obj_key);

	/* add server object to object pool */
	obj->cap(new_obj_cap);
	insert(obj);

	return reinterpret_cap_cast<Pager_object>(new_obj_cap);
}


