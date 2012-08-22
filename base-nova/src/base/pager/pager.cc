/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
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

enum { PF_HANDLER_STACK_SIZE = sizeof(addr_t) * 1024 };
extern Genode::addr_t __core_pd_sel;

void Pager_object::_page_fault_handler()
{
	Ipc_pager ipc_pager;
	ipc_pager.wait_for_fault();

	/* serialize page-fault handling */
	Thread_base *myself = Thread_base::myself();
	if (!myself) {
		PWRN("unexpected page-fault for non-existing pager object,"
		     " going to sleep forever");
		sleep_forever();
	}

	Pager_object *obj = static_cast<Pager_object *>(myself);
	int ret = obj->pager(ipc_pager);

	if (ret) {
		if (!obj->submit_exception_signal()) {
			PWRN("unresolvable page-fault at address 0x%lx, ip=0x%lx",
		    	 ipc_pager.fault_addr(), ipc_pager.fault_ip());

			/* revoke paging capability, let thread die in kernel */
			Nova::revoke(Obj_crd(obj->exc_pt_sel() + PT_SEL_PAGE_FAULT, 0),
			             true);
			obj->_state.dead = true;
		} else
			/* Somebody takes care don't die - just recall and block */
			 obj->client_recall();

		Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
		utcb->set_msg_word(0);
		utcb->mtd = 0;
	}

	ipc_pager.reply_and_wait_for_fault();
}

void Pager_object::_recall_handler()
{
	Pager_object *obj = static_cast<Pager_object *>(Thread_base::myself());
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();

	obj->_copy_state(utcb);

	obj->_state.thread.ip     = utcb->ip;
	obj->_state.thread.sp     = utcb->sp;

	obj->_state.thread.eflags = utcb->flags;
	obj->_state.thread.trapno = PT_SEL_RECALL;

	obj->_state.valid         = true;

	if (sm_ctrl(obj->_sm_state_notify, SEMAPHORE_UP) != NOVA_OK)
		PWRN("notify failed");

	if (sm_ctrl(obj->exc_pt_sel() + SM_SEL_EC, SEMAPHORE_DOWNZERO) != NOVA_OK)
		PWRN("blocking recall handler failed");

	obj->_state.valid = false;

	utcb->set_msg_word(0);
	utcb->mtd = 0;
	reply(Thread_base::myself()->stack_top());
}

void Pager_object::_startup_handler()
{
	Pager_object *obj = static_cast<Pager_object *>(Thread_base::myself());
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();

	utcb->ip  = obj->_initial_eip;
	utcb->sp  = obj->_initial_esp;
	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);
	reply(Thread_base::myself()->stack_top());
}


void Pager_object::_invoke_handler()
{
	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
	Pager_object *obj = static_cast<Pager_object *>(Thread_base::myself());

	/* send single portal as reply */
	addr_t event = utcb->msg_words() != 1 ? 0 : utcb->msg[0];
	utcb->mtd = 0;
	utcb->set_msg_word(0);

	if (event == PT_SEL_STARTUP || event == PT_SEL_PAGE_FAULT ||
	    event == SM_SEL_EC || event == PT_SEL_RECALL) {

		/**
		 * Caller is requesting the SM cap of thread
		 * this object is paging - it is stored at SM_SEL_EC_CLIENT
		 */
		if (event == SM_SEL_EC) event = SM_SEL_EC_CLIENT;

		bool res = utcb->append_item(Obj_crd(obj->exc_pt_sel() + event,
		                                     0), 0);
		/* one item ever fits on the UTCB */
		(void)res;
	}

	reply(Thread_base::myself()->stack_top());
}


void Pager_object::wake_up() { cancel_blocking(); }


void Pager_object::client_cancel_blocking() {
	uint8_t res = sm_ctrl(exc_pt_sel() + SM_SEL_EC_CLIENT, SEMAPHORE_UP);
	if (res != NOVA_OK)
		PWRN("cancel blocking failed");
}


uint8_t Pager_object::client_recall() {
	return ec_ctrl(_state.sel_client_ec);
}


Pager_object::Pager_object(unsigned long badge)
: Thread_base("pager", PF_HANDLER_STACK_SIZE), _badge(badge)
{
	_pt_cleanup          = cap_selector_allocator()->alloc();
	_sm_state_notify     = cap_selector_allocator()->alloc();
	_state.valid         = false;
	_state.dead          = false;
	_state.sel_client_ec = ~0UL;

	/* create portal for page-fault handler */
	addr_t pd_sel = __core_pd_sel;
	uint8_t res = create_pt(exc_pt_sel() + PT_SEL_PAGE_FAULT, pd_sel,
	                        _tid.ec_sel, Mtd(Mtd::QUAL | Mtd::EIP),
	                        (mword_t)_page_fault_handler);
	if (res) {
		PERR("could not create page-fault portal, error = %u\n",
		     res);
		class Create_page_fault_pt_failed { };
		throw Create_page_fault_pt_failed();
	}

	/* create portal for startup handler */
	res = create_pt(exc_pt_sel() + PT_SEL_STARTUP, pd_sel, _tid.ec_sel,
	                Mtd(Mtd::ESP | Mtd::EIP), (mword_t)_startup_handler);
	if (res) {
		PERR("could not create startup portal, error = %u\n",
		     res);
		class Create_startup_pt_failed { };
		throw Create_startup_pt_failed();
	}

	/* Create portal for recall handler */
	Mtd mtd(Mtd::ESP | Mtd::EIP | Mtd::ACDB | Mtd::EFL | Mtd::EBSD | Mtd::FSGS);
	res = create_pt(exc_pt_sel() + PT_SEL_RECALL, pd_sel, _tid.ec_sel,
	                mtd, (addr_t)_recall_handler);
	if (res) {
		PERR("could not create recall portal, error = %u\n", res);
		class Create_recall_pt_failed { };
		throw Create_recall_pt_failed();
	}

	/* Create portal for final cleanup call used during destruction */
	res = create_pt(_pt_cleanup, pd_sel, _tid.ec_sel, Mtd(0),
	                reinterpret_cast<addr_t>(_invoke_handler));
	if (res) {
		PERR("could not create pager cleanup portal, error = %u\n", res);
		class Create_cleanup_pt_failed { };
		throw Create_cleanup_pt_failed();
	}

	res = Nova::create_sm(_sm_state_notify, pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		class Create_state_notifiy_sm_failed { };
		throw Create_state_notifiy_sm_failed();
	}
}

Pager_object::~Pager_object()
{
	/* Revoke portals of Pager_object */
	revoke(Obj_crd(exc_pt_sel() + PT_SEL_STARTUP, 0), true);
	revoke(Obj_crd(exc_pt_sel() + PT_SEL_RECALL, 0), true);
	revoke(Obj_crd(exc_pt_sel() + PT_SEL_PAGE_FAULT, 0), true);

	/* Revoke semaphore cap to signal valid state after recall */
	addr_t sm_cap = _sm_state_notify;
	_sm_state_notify = Native_thread::INVALID_INDEX;
	/* If pager is blocked wake him up */
	sm_ctrl(sm_cap, SEMAPHORE_UP);
	revoke(Obj_crd(sm_cap, 0), true);

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
	cap_selector_allocator()->free(_pt_cleanup, 0);
	cap_selector_allocator()->free(sm_cap, 0);
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* request creation of portal bind to pager thread */
	Native_capability pager_thread_cap(obj->ec_sel());
	Native_capability cap_session =
		_cap_session->alloc(pager_thread_cap, obj->handler_address());

	/* add server object to object pool */
	obj->Object_pool<Pager_object>::Entry::cap(cap_session);
	insert(obj);

	/* return capability that uses the object id as badge */
	return reinterpret_cap_cast<Pager_object>(
		obj->Object_pool<Pager_object>::Entry::cap());
}


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	/* cleanup at cap session */
	_cap_session->free(obj->Object_pool<Pager_object>::Entry::cap());

	/* cleanup locally */
	Native_capability pager_pt =
		obj->Object_pool<Pager_object>::Entry::cap();

	revoke(pager_pt.dst(), true);

	cap_selector_allocator()->free(pager_pt.local_name(), 0);

	remove(obj);
}

