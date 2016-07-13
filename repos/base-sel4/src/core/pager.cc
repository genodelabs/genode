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

/* core includes */
#include <pager.h>
#include <core_capability_space.h>
#include <install_mapping.h>
#include <platform.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

/* seL4 includes */
#include <sel4/sel4.h>
#include <sel4/arch/pfIPC.h>

using namespace Genode;


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
	{ }
};


/***************
 ** IPC pager **
 ***************/


void Ipc_pager::wait_for_fault()
{
	if (_last && _reply_sel) {
		seL4_CNode const service = seL4_CapInitThreadCNode;
		seL4_Word  const index   = _reply_sel;
		uint8_t    const depth   = 32;
		int ret = seL4_CNode_SaveCaller(service, index, depth);
		if (ret != seL4_NoError)
			Genode::error("saving reply cap failed with ", ret);
	}
	_reply_sel = 0;
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


Ipc_pager::Ipc_pager() : _last(0), _reply_sel(0) { }


/******************
 ** Pager object **
 ******************/

Pager_object::Pager_object(Cpu_session_capability cpu_sesion,
                           Thread_capability thread,
                           unsigned long badge, Affinity::Location location)
:
	_badge(badge), _cpu_session_cap(cpu_sesion), _thread_cap(thread),
	_reply_cap(platform_specific()->core_sel_alloc().alloc())
{ }


Pager_object::~Pager_object()
{
	seL4_CNode_Delete(seL4_CapInitThreadCNode, _reply_cap.value(), 32);
	platform_specific()->core_sel_alloc().free(_reply_cap);
	/* invalidate reply cap for Pager_object::wait_for_fault() _reply_sel */
	_reply_cap = Cap_sel(0);
}


void Pager_object::wake_up()
{
	seL4_MessageInfo_t const send_msg = seL4_MessageInfo_new(0, 0, 0, 0);
	seL4_Send(_reply_cap.value(), send_msg);
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


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	using Pool = Object_pool<Pager_object>;

	if (obj) Pool::remove(obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	Native_capability cap = _pager_object_cap(obj->badge());

	/* add server object to object pool */
	obj->cap(cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return reinterpret_cap_cast<Pager_object>(cap);
}


void Pager_entrypoint::entry()
{
	using Pool = Object_pool<Pager_object>;

	bool reply_pending = false;

	while (1) {

		if (reply_pending)
			_pager.reply_and_wait_for_fault();
		else
			_pager.wait_for_fault();

		reply_pending = false;

		Pool::apply(_pager.badge(), [&] (Pager_object *obj) {
			if (!obj)
				return;

			/* send reply if page-fault handling succeeded */
			reply_pending = !obj->pager(_pager);
			if (!reply_pending)
				_pager.reply_save_caller(obj->reply_cap_sel());
		});
	}
}
