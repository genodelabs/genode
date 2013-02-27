/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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
		if (obj->client_recall() != Nova::NOVA_OK) {
			char client_name[Context::NAME_LEN];
			myself->name(client_name, sizeof(client_name));

			PWRN("unresolvable page fault since recall failed, '%s' "
			     "address=0x%lx ip=0x%lx", client_name, ipc_pager.fault_addr(),
			     ipc_pager.fault_ip());

			Native_capability pager_obj = obj->Object_pool<Pager_object>::Entry::cap();
			revoke(pager_obj.dst(), true);

			revoke(Obj_crd(obj->exc_pt_sel(), NUM_INITIAL_PT_LOG2), false);

			obj->_state.dead = true;
		}

		if (ret == 1)
			PDBG("unhandled page fault, address=0x%lx ip=0x%lx",
			     ipc_pager.fault_addr(), ipc_pager.fault_ip());

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
	addr_t fault_ip    = utcb->ip;

	if (obj->submit_exception_signal()) {
		/* somebody takes care don't die - just recall and block */
		if (obj->client_recall() != Nova::NOVA_OK) {
			PERR("recall failed exception_handler");
			nova_die();
		}
	}
	else {
		char client_name[Context::NAME_LEN];
		myself->name(client_name, sizeof(client_name));

		PWRN("unresolvable exception at ip 0x%lx, exception portal 0x%lx, "
		     "'%s'", fault_ip, portal_id, client_name);

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

		/*
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


void Pager_object::client_cancel_blocking()
{
	uint8_t res = sm_ctrl(exc_pt_sel() + SM_SEL_EC_CLIENT, SEMAPHORE_UP);
	if (res != NOVA_OK)
		PWRN("cancel blocking failed");
}


uint8_t Pager_object::client_recall()
{
	return ec_ctrl(_state.sel_client_ec);
}


void Pager_object::cleanup_call()
{
	/*
	 * Revoke all portals of Pager_object from others.
	 * The portals will be finally revoked during thread destruction.
	 */
	revoke(Obj_crd(exc_pt_sel(), NUM_INITIAL_PT_LOG2), false);

	Utcb *utcb = (Utcb *)Thread_base::myself()->utcb();
	if (reinterpret_cast<Utcb *>(this->utcb()) == utcb) return;

	/* if pager is blocked wake him up */
	wake_up();

	utcb->set_msg_word(0);
	if (uint8_t res = call(_pt_cleanup))
		PERR("%8p - cleanup call to pager (%8p) failed res=%d",
		     utcb, this->utcb(), res);
}

static uint8_t create_portal(addr_t pt, addr_t pd, addr_t ec, Mtd mtd,
	                         addr_t eip)
{
	uint8_t res = create_pt(pt, pd, ec, mtd, eip);

	if (res == NOVA_OK)
		revoke(Obj_crd(pt, 0, Obj_crd::RIGHT_PT_CTRL));

	return res;	
}

Pager_object::Pager_object(unsigned long badge)
: Thread_base("pager:", PF_HANDLER_STACK_SIZE), _badge(badge)
{
	class Create_exception_pt_failed { };
	uint8_t res;

	/* construct pager name out of client name */
	strncpy(_context->name + 6, reinterpret_cast<char const *>(badge),
	        sizeof(_context->name) - 6);

	addr_t pd_sel        = __core_pd_sel;
	_pt_cleanup          = cap_selector_allocator()->alloc();
	_sm_state_notify     = cap_selector_allocator()->alloc();
	_state.valid         = false;
	_state.dead          = false;
	_state.singlestep    = false;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;

	/* creates local EC */
	Thread_base::start();

	/* create portal for exception handlers 0x0 - 0xd */
	for (unsigned i = 0; i < PT_SEL_PAGE_FAULT; i++) {
		res = create_portal(exc_pt_sel() + i, pd_sel, _tid.ec_sel, Mtd(0),
		                    (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
	}

	/* threads in core get default page fault handler assigned, revoke it */
	if (_tid.ec_sel != Native_thread::INVALID_INDEX)
		revoke(Obj_crd(exc_pt_sel() + PT_SEL_PAGE_FAULT, 0));

	/* create portal for page-fault handler */
	res = create_portal(exc_pt_sel() + PT_SEL_PAGE_FAULT, pd_sel, _tid.ec_sel,
	                    Mtd(Mtd::QUAL | Mtd::EIP), (mword_t)_page_fault_handler);
	if (res) {
		PERR("could not create page-fault portal, error = %u\n", res);
		class Create_page_fault_pt_failed { };
		throw Create_page_fault_pt_failed();
	}

	/* create portal for exception handlers 0xf - 0x19 */
	for (unsigned i = PT_SEL_PAGE_FAULT + 1; i < PT_SEL_PARENT; i++) {
		res = create_portal(exc_pt_sel() + i, pd_sel, _tid.ec_sel, Mtd(0),
		                    (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
	}

	/* create portal for startup handler */
	res = create_portal(exc_pt_sel() + PT_SEL_STARTUP, pd_sel, _tid.ec_sel,
	                    Mtd(Mtd::ESP | Mtd::EIP), (mword_t)_startup_handler);
	if (res) {
		PERR("could not create startup portal, error = %u\n",
		     res);
		class Create_startup_pt_failed { };
		throw Create_startup_pt_failed();
	}

	/* create portal for recall handler */
	Mtd mtd(Mtd::ESP | Mtd::EIP | Mtd::ACDB | Mtd::EFL | Mtd::EBSD | Mtd::FSGS);
	res = create_portal(exc_pt_sel() + PT_SEL_RECALL, pd_sel, _tid.ec_sel,
	                    mtd, (addr_t)_recall_handler);
	if (res) {
		PERR("could not create recall portal, error = %u\n", res);
		class Create_recall_pt_failed { };
		throw Create_recall_pt_failed();
	}

	/* create portal for final cleanup call used during destruction */
	res = create_portal(_pt_cleanup, pd_sel, _tid.ec_sel, Mtd(0),
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
	/* revoke semaphore cap to signal valid state after recall */
	addr_t sm_cap    = _sm_state_notify;
	_sm_state_notify = Native_thread::INVALID_INDEX;

	/* if pager is blocked wake him up */
	sm_ctrl(sm_cap, SEMAPHORE_UP);
	revoke(Obj_crd(sm_cap, 0));

	/* take care nobody is handled anymore by this object */
	cleanup_call();

	/* revoke portal used for the cleanup call */
	revoke(Obj_crd(_pt_cleanup, 0));

	Native_capability pager_obj = ::Object_pool<Pager_object>::Entry::cap();
	cap_selector_allocator()->free(_pt_cleanup, 0);
	cap_selector_allocator()->free(sm_cap, 0);
	cap_selector_allocator()->free(pager_obj.local_name(), 0);
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
	Native_capability pager_obj = obj->Object_pool<Pager_object>::Entry::cap();

	/* cleanup at cap session */
	_cap_session->free(pager_obj);

	/* cleanup locally */
	revoke(pager_obj.dst(), true);

	remove_locked(obj);
	obj->cleanup_call();
}

