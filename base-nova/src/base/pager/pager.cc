/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/cap_sel_alloc.h>
#include <base/pager.h>
#include <base/sleep.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;
using namespace Nova;

enum { PF_HANDLER_STACK_SIZE = 4096 };

void Pager_object::_page_fault_handler()
{
	Ipc_pager ipc_pager;
	ipc_pager.wait_for_fault();

	/* serialize page-fault handling */
	Thread_base *myself = Thread_base::myself();
	if (!myself) {
		PWRN("unexpected page-fault for non-existing pager object, going to sleep forever");
		sleep_forever();
	}

	Pager_object *obj = static_cast<Pager_object *>(myself);
	int ret = obj->pager(ipc_pager);

	if (ret) {
		PWRN("page-fault resolution for address 0x%lx, ip=0x%lx failed",
		     ipc_pager.fault_addr(), ipc_pager.fault_ip());
		/* revoke paging capability */
		Nova::revoke(Nova::Obj_crd(obj->exc_pt_sel() + PT_SEL_PAGE_FAULT, 0), true);
		Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
		utcb->set_msg_word(0);
	}

	ipc_pager.reply_and_wait_for_fault();
}


void Pager_object::_startup_handler()
{
	Pager_object *obj = static_cast<Pager_object *>(Thread_base::myself());
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();

	printf("start new pager object with EIP=0x%p, ESP=0x%p\n",
	       (void *)obj->_initial_eip, (void *)obj->_initial_esp);

	utcb->eip = obj->_initial_eip;
	utcb->esp = obj->_initial_esp;
	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);
	reply(Thread_base::myself()->stack_top());
}


void Pager_object::_invoke_handler()
{
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
	Pager_object *obj = static_cast<Pager_object *>(Thread_base::myself());

	/* send single portal as reply */
	addr_t event = utcb->msg[0];
	utcb->mtd = 0;
	utcb->set_msg_word(0);

	if (event == PT_SEL_STARTUP || event == PT_SEL_PAGE_FAULT)
		utcb->append_item(Obj_crd(obj->exc_pt_sel() + event, 0), 0);

	reply(Thread_base::myself()->stack_top());
}


void Pager_object::wake_up() { PDBG("not yet implemented"); }


Pager_object::Pager_object(unsigned long badge)
: Thread_base("pager", PF_HANDLER_STACK_SIZE), _badge(badge)
{
	addr_t pd_sel = cap_selector_allocator()->pd_sel();

	enum { CPU_NO = 0, GLOBAL = false, EXC_BASE = 0 };

	mword_t *thread_sp   = (mword_t *)&_context->stack[-4];
	mword_t  thread_utcb = (mword_t)  &_context->utcb;

	/* create local EC */
	uint8_t res = create_ec(_tid.ec_sel, pd_sel,
	                        CPU_NO, thread_utcb,
	                        (mword_t)thread_sp, /* <- delivered to the startup handler */
	                        EXC_BASE, GLOBAL);
	if (res)
		PERR("create_ec returned %d - utcb 0x%lx", res, thread_utcb);

	/* create portal for page-fault handler */
	res = create_pt(exc_pt_sel() + PT_SEL_PAGE_FAULT, pd_sel, _tid.ec_sel,
	                Mtd(Mtd::QUAL | Mtd::EIP), (mword_t)_page_fault_handler);
	if (res) {
		PERR("could not create page-fault portal, create_pt returned %d\n",
		     res);
		class Create_page_fault_pt_failed { };
		throw Create_page_fault_pt_failed();
	}

	/* create portal for startup handler */
	res = create_pt(exc_pt_sel() + PT_SEL_STARTUP, pd_sel, _tid.ec_sel,
	                Mtd(Mtd::ESP | Mtd::EIP), (mword_t)_startup_handler);
	if (res) {
		PERR("could not create startup portal, create_pt returned %d\n",
		     res);
		class Create_startup_pt_failed { };
		throw Create_startup_pt_failed();
	}

	/*
	 * Create object identity representing the pager object. It is used as
	 * argument to 'Cpu_session::set_pager'. Furthermore it can be invoked to
	 * request the mapping of the event capability selector window
	 * corresponding to the pager object.
	 */
	_pt_sel = cap_selector_allocator()->alloc();
	res = create_pt(_pt_sel, pd_sel, _tid.ec_sel, Mtd(0), (mword_t)_invoke_handler);
	if (res)
		PERR("could not create pager object identity, create_pt returned %d\n", res);

	_pt_cleanup = cap_selector_allocator()->alloc();
	res = create_pt(_pt_cleanup, pd_sel, _tid.ec_sel, Mtd(0), (mword_t)_invoke_handler);
	if (res)
		PERR("could not create pager cleanup portal, create_pt returned %d\n", res);
}

Pager_object::~Pager_object()
{
	/* Revoke thread portals serving exceptions */
	revoke(Obj_crd(exc_pt_sel(), NUM_INITIAL_PT_LOG2), true);
	/* Revoke portal used as identity object */
	revoke(Obj_crd(_pt_sel, 0), true);

	/* Make sure nobody is in the handler anymore by doing an IPC to a
	 * local cap pointing to same serving thread (if not running in the
	 * context of the serving thread). When the call returns
	 * we know that nobody is handled by this object anymore, because
	 * all remotely available portals had been revoked beforehand.
	 */
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
	if (reinterpret_cast<Utcb *>(&_context->utcb) != utcb) {
		utcb->set_msg_word(0);
		if (uint8_t res = call(_pt_cleanup))
			PERR("failure - cleanup call failed res=%d", res);
	}

	/* Revoke portal used for the cleanup call */
	revoke(Obj_crd(_pt_cleanup, 0), true);

	cap_selector_allocator()->free(_pt_sel, 0);
	cap_selector_allocator()->free(_pt_cleanup, 0);

}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* supplement capability with object ID obtained from CAP session */
	obj->Object_pool<Pager_object>::Entry::cap(_cap_session->alloc(Native_capability(obj->pt_sel(), 0)));

	/* add server object to object pool */
	insert(obj);

	/* return capability that uses the object id as badge */
	return reinterpret_cap_cast<Pager_object>(obj->Object_pool<Pager_object>::Entry::cap());
}


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	/* cleanup cap session */
	_cap_session->free(obj->Object_pool<Pager_object>::Entry::cap());
	if (obj->pt_sel() != 0UL + obj->Object_pool<Pager_object>::Entry::cap().dst()) {
		cap_selector_allocator()->free(obj->Object_pool<Pager_object>::Entry::cap().dst(), 0);
		revoke(Obj_crd(obj->Object_pool<Pager_object>::Entry::cap().dst(), 0), true);
	}

	remove(obj);
}

