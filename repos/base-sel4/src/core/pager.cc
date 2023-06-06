/*
 * \brief  Pager
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pager.h>
#include <core_capability_space.h>
#include <install_mapping.h>
#include <platform.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

#include "fault_info.h"

/* seL4 includes */
#include <sel4/sel4.h>

using namespace Core;


void Mapping::prepare_map_operation() const { }


/***************
 ** IPC pager **
 ***************/

void Ipc_pager::wait_for_fault()
{
	if (_badge && _reply_sel) {
		seL4_CNode const service = seL4_CapInitThreadCNode;
		seL4_Word  const index   = _reply_sel;
		uint8_t    const depth   = 32;
		int ret = seL4_CNode_SaveCaller(service, index, depth);
		if (ret != seL4_NoError)
			error("saving reply cap failed with ", ret);
	}
	_reply_sel = 0;
	_badge = 0;
	reply_and_wait_for_fault();
}


bool Ipc_pager::install_mapping()
{
	_badge = Core::install_mapping(_reply_mapping, _badge);
	return _badge;
}


void Ipc_pager::reply_and_wait_for_fault()
{
	seL4_Word badge = Rpc_obj_key::INVALID;

	seL4_MessageInfo_t page_fault_msg_info;

	if (_badge) {

		seL4_MessageInfo_t const reply_msg = seL4_MessageInfo_new(0, 0, 0, 0);

		page_fault_msg_info =
			seL4_ReplyRecv(Thread::myself()->native_thread().ep_sel, reply_msg, &badge);

	} else {
		page_fault_msg_info =
			seL4_Recv(Thread::myself()->native_thread().ep_sel, &badge);
	}

	Fault_info const fault_info(page_fault_msg_info);

	_pf_ip      = fault_info.ip;
	_pf_addr    = fault_info.pf;
	_pf_write   = fault_info.write;
	_pf_exec    = fault_info.exec_fault();
	_pf_align   = fault_info.align_fault();
	_badge      = badge;

	addr_t const fault_type = seL4_MessageInfo_get_label(page_fault_msg_info);

	_exception = fault_type != seL4_Fault_VMFault;

	auto fault_name = [] (addr_t type)
	{
		switch (type) {
		case seL4_Fault_NullFault:      return "seL4_Fault_NullFault";
		case seL4_Fault_CapFault:       return "seL4_Fault_CapFault";
		case seL4_Fault_UnknownSyscall: return "seL4_Fault_UnknownSyscall";
		case seL4_Fault_UserException:  return "seL4_Fault_UserException";
		case seL4_Fault_VMFault:        return "seL4_Fault_VMFault";
		}
		return "unknown";
	};

	if (fault_type != seL4_Fault_VMFault)
		error("unexpected exception during fault '", fault_name(fault_type), "'");
}


Ipc_pager::Ipc_pager() { }


/******************
 ** Pager object **
 ******************/

Pager_object::Pager_object(Cpu_session_capability cpu_session,
                           Thread_capability thread,
                           unsigned long badge, Affinity::Location,
                           Session_label const &pd_label,
                           Cpu_session::Name const &name)
:
	_badge(badge), _cpu_session_cap(cpu_session), _thread_cap(thread),
	_reply_cap(platform_specific().core_sel_alloc().alloc()),
	_pd_label(pd_label), _name(name)
{ }


Pager_object::~Pager_object()
{
	seL4_CNode_Delete(seL4_CapInitThreadCNode, _reply_cap.value(), 32);
	platform_specific().core_sel_alloc().free(_reply_cap);
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


void Pager_entrypoint::dissolve(Pager_object &obj)
{
	using Pool = Object_pool<Pager_object>;

	Pool::remove(&obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object &obj)
{
	Native_capability cap = _pager_object_cap(obj.badge());

	/* add server object to object pool */
	obj.cap(cap);
	insert(&obj);

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

			/* on exception (beside page fault) don't reply and submit signal */
			if (_pager.exception()) {
				warning("exception ", _pager.fault_addr(), " ",
				        *obj, " ip=", Hex(_pager.fault_ip()));
				obj->submit_exception_signal();
				return;
			}

			/* on alignment fault don't reply and submit signal */
			if (_pager.align_fault()) {
				warning("alignment fault, addr=", Hex(_pager.fault_addr()),
				        " ip=", Hex(_pager.fault_ip()));
				reply_pending = false;
				obj->submit_exception_signal();
				return;
			}

			/* send reply if page-fault handling succeeded */
			reply_pending = !obj->pager(_pager);
			if (!reply_pending) {
				warning("page-fault, ", *obj,
				        " ip=", Hex(_pager.fault_ip()),
				        " pf-addr=", Hex(_pager.fault_addr()));
				_pager.reply_save_caller(obj->reply_cap_sel());
				return;
			}

			/* install memory mappings */
			if (_pager.install_mapping()) {
				return;
			}
		});
	}
}
