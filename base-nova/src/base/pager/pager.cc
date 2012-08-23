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

Utcb * Pager_object::_check_handler(Thread_base *&myself, Pager_object *&obj)
{
	Utcb * utcb;
	myself = Thread_base::myself();
	obj    = static_cast<Pager_object *>(myself);

	if (!myself || !obj) goto dead;

	utcb   = reinterpret_cast<Utcb *>(myself->utcb());
	if (!utcb) goto dead;

	return utcb;

	dead:

	PERR("unexpected exception-fault for non-existing pager object,"
	     " going to sleep forever");

	if (obj) obj->_state.dead = true;
	sleep_forever();
}

void Pager_object::_page_fault_handler()
{
	Ipc_pager ipc_pager;
	ipc_pager.wait_for_fault();

	Thread_base  *myself;
	Pager_object *obj;
	Utcb         *utcb = _check_handler(myself, obj);

	int ret = obj->pager(ipc_pager);

	if (ret) {
		if (obj->submit_exception_signal())
			/* Somebody takes care don't die - just recall and block */
			obj->client_recall();
		else {
			PWRN("unresolvable page-fault at address 0x%lx, ip=0x%lx",
		    	 ipc_pager.fault_addr(), ipc_pager.fault_ip());

			/* revoke paging capability, let thread die in kernel */
			Nova::revoke(Obj_crd(obj->exc_pt_sel() + PT_SEL_PAGE_FAULT, 0));
			obj->_state.dead = true;
		}

		utcb->set_msg_word(0);
		utcb->mtd = 0;
	}

	ipc_pager.reply_and_wait_for_fault();
}

void Pager_object::_exception_handler(addr_t portal_id)
{
	Thread_base  *myself;
	Pager_object *obj;
	Utcb         *utcb = _check_handler(myself, obj);

	if (obj->submit_exception_signal()) 
		/* Somebody takes care don't die - just recall and block */
		obj->client_recall();
	else {
		Nova::revoke(Obj_crd(portal_id, 0));
		obj->_state.dead = true;
	}

	utcb->set_msg_word(0);
	utcb->mtd = 0;

	reply(myself->stack_top());
}

void Pager_object::_recall_handler()
{
	Thread_base  *myself;
	Pager_object *obj;
	Utcb         *utcb = _check_handler(myself, obj);

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

	bool singlestep_state = obj->_state.thread.eflags & 0x100UL;
	if (obj->_state.singlestep && !singlestep_state) {
		utcb->flags = obj->_state.thread.eflags | 0x100UL;
		utcb->mtd = Nova::Mtd(Mtd::EFL).value();
	} else
	if (!obj->_state.singlestep && singlestep_state) {
		utcb->flags = obj->_state.thread.eflags & ~0x100UL;
		utcb->mtd = Nova::Mtd(Mtd::EFL).value();
	} else
		utcb->mtd = 0;
	utcb->set_msg_word(0);
	
	reply(myself->stack_top());
}

void Pager_object::_startup_handler()
{
	Thread_base  *myself;
	Pager_object *obj;
	Utcb         *utcb = _check_handler(myself, obj);

	utcb->ip  = obj->_initial_eip;
	utcb->sp  = obj->_initial_esp;

	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);

	reply(myself->stack_top());
}


void Pager_object::_invoke_handler()
{
	Thread_base  *myself;
	Pager_object *obj;
	Utcb         *utcb = _check_handler(myself, obj);

	/* send single portal as reply */
	addr_t event = utcb->msg_words() != 1 ? 0 : utcb->msg[0];
	utcb->mtd = 0;
	utcb->set_msg_word(0);

	if (event < PT_SEL_PARENT || event == PT_SEL_STARTUP ||
	    event == SM_SEL_EC    || event == PT_SEL_RECALL) {

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

	reply(myself->stack_top());
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
	class Create_exception_pt_failed { };
	uint8_t res;

	addr_t pd_sel        = __core_pd_sel;
	_pt_cleanup          = cap_selector_allocator()->alloc();
	_sm_state_notify     = cap_selector_allocator()->alloc();
	_state.valid         = false;
	_state.dead          = false;
	_state.singlestep    = false;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;

	/* Create portal for exception handlers 0x0 - 0xd */
	for (unsigned i = 0; i < PT_SEL_PAGE_FAULT; i++) {
		res = create_pt(exc_pt_sel() + i, pd_sel, _tid.ec_sel,
		                Mtd(0), (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
	}

	/* create portal for page-fault handler */
	res = create_pt(exc_pt_sel() + PT_SEL_PAGE_FAULT, pd_sel,
	                _tid.ec_sel, Mtd(Mtd::QUAL | Mtd::EIP),
	                (mword_t)_page_fault_handler);
	if (res) {
		PERR("could not create page-fault portal, error = %u\n",
		     res);
		class Create_page_fault_pt_failed { };
		throw Create_page_fault_pt_failed();
	}

	/* Create portal for exception handlers 0xf - 0x19 */
	for (unsigned i = PT_SEL_PAGE_FAULT + 1; i < PT_SEL_PARENT; i++) {
		res = create_pt(exc_pt_sel() + i, pd_sel, _tid.ec_sel,
		                Mtd(0), (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
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
	/**
	 * Revoke all portals of Pager_object from others.
	 * The portals will be finally revoked during thread destruction.
	 */
	revoke(Obj_crd(exc_pt_sel(), NUM_INITIAL_PT_LOG2), false);

	/* Revoke semaphore cap to signal valid state after recall */
	addr_t sm_cap = _sm_state_notify;
	_sm_state_notify = Native_thread::INVALID_INDEX;
	/* If pager is blocked wake him up */
	sm_ctrl(sm_cap, SEMAPHORE_UP);
	revoke(Obj_crd(sm_cap, 0));

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
	revoke(Obj_crd(_pt_cleanup, 0));
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

