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

	if (obj) obj->_state.mark_dead();
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

			revoke(Obj_crd(obj->exc_pt_sel_client(), NUM_INITIAL_PT_LOG2), false);

			obj->_state.mark_dead();
		}

		if (ret == 1) {
			obj->_state.thread.unresolved_page_fault = true;

			char client_name[Context::NAME_LEN];
			myself->name(client_name, sizeof(client_name));

			PDBG("unhandled page fault, '%s' address=0x%lx ip=0x%lx",
			     client_name, ipc_pager.fault_addr(), ipc_pager.fault_ip());
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
	addr_t fault_ip    = utcb->ip;
	uint8_t res        = 0xFF;

	if (obj->submit_exception_signal())
		res = obj->client_recall();

	if (res != NOVA_OK) {
		char client_name[Context::NAME_LEN];
		myself->name(client_name, sizeof(client_name));

		PWRN("unresolvable exception at ip 0x%lx, exception portal 0x%lx, %s, "
		     "'%s'", fault_ip,
		     portal_id, res == 0xFF ? "no signal handler" :
		     res == NOVA_OK ? "" : "recall failed",
		     client_name);

		Nova::revoke(Obj_crd(portal_id, 0));
		obj->_state.mark_dead();
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

	obj->_state.mark_valid();

	if (obj->_state.is_client_cancel())
		if (sm_ctrl(obj->sm_state_notify(), SEMAPHORE_UP) != NOVA_OK)
			PWRN("notify failed");

	do {
		if (sm_ctrl(obj->exc_pt_sel() + SM_SEL_EC, SEMAPHORE_DOWNZERO) != NOVA_OK)
			PWRN("blocking recall handler failed");
	} while (obj->_state.is_client_cancel());

	obj->_state.mark_invalid();

	bool singlestep_state = obj->_state.thread.eflags & 0x100UL;
	if (obj->_state.singlestep() && !singlestep_state) {
		utcb->flags = obj->_state.thread.eflags | 0x100UL;
		utcb->mtd = Nova::Mtd(Mtd::EFL).value();
	} else
		if (!obj->_state.singlestep() && singlestep_state) {
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
	addr_t const event    = utcb->msg[0];
	addr_t const logcount = utcb->msg[1];

	utcb->mtd = 0;
	utcb->set_msg_word(0);

	/* native ec cap requested */
	if (event == ~0UL) {
		/**
		 * Return native EC cap with specific rights mask set.
		 * If the cap is mapped the kernel will demote the
		 * rights of the EC as specified by the rights mask.
		 *
		 * The cap is supposed to be returned to clients,
		 * which they have to use as argument to identify
		 * the thread to which they want attach portals.
		 *
		 * The demotion by the kernel during the map operation
		 * takes care that the EC cap itself contains
		 * no usable rights for the clients.
		 */
		bool res = utcb->append_item(Obj_crd(obj->_state.sel_client_ec, 0,
		                                     Obj_crd::RIGHT_EC_RECALL), 0);
		res = utcb->append_item(Obj_crd(obj->Object_pool<Pager_object>::Entry::cap().local_name(), 0), 1);
		(void)res;

		reply(myself->stack_top());
	}

	/* semaphore for signaling thread is requested, reuse PT_SEL_STARTUP. */
	if (event == ~0UL - 1) {
		/* create semaphore only once */
		if (!obj->_state.has_signal_sm()) {

			revoke(Obj_crd(obj->exc_pt_sel_client() + PT_SEL_STARTUP, 0));

			bool res = Nova::create_sm(obj->exc_pt_sel_client() + PT_SEL_STARTUP,
			                           __core_pd_sel, 0);
			if (res != Nova::NOVA_OK)
				reply(myself->stack_top());

			obj->_state.mark_signal_sm();
		}

		bool res = utcb->append_item(Obj_crd(obj->exc_pt_sel_client() +
		                                     PT_SEL_STARTUP, 0), 0);
		(void)res;

		reply(myself->stack_top());
	}

	/* sanity check, if event is not valid return nothing */
	if (logcount > NUM_INITIAL_PT_LOG2 || event > 1UL << NUM_INITIAL_PT_LOG2 ||
	    event + (1UL << logcount) > (1UL << NUM_INITIAL_PT_LOG2))
		reply(myself->stack_top());

	/* valid event portal is requested, delegate it to caller */
	bool res = utcb->append_item(Obj_crd(obj->exc_pt_sel_client() + event,
	                                     logcount), 0);
	(void)res;

	reply(myself->stack_top());
}


void Pager_object::wake_up()
{
	_state.unmark_client_cancel();

	cancel_blocking();
}


void Pager_object::client_cancel_blocking()
{
	if (_state.is_client_cancel())
		return;

	_state.mark_client_cancel();

	uint8_t res = sm_ctrl(exc_pt_sel_client() + SM_SEL_EC, SEMAPHORE_UP);
	if (res != NOVA_OK)
		PWRN("canceling blocked client failed (thread sm)");

	if (!_state.has_signal_sm())
		return;

	res = sm_ctrl(exc_pt_sel_client() + PT_SEL_STARTUP, SEMAPHORE_UP);
	if (res != NOVA_OK)
		PWRN("canceling blocked client failed (signal sm)");
}


uint8_t Pager_object::client_recall()
{
	return ec_ctrl(_state.sel_client_ec);
}


void Pager_object::cleanup_call()
{
	/* revoke all portals handling the client. */
	revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

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


Pager_object::Pager_object(unsigned long badge, Affinity::Location location)
: Thread_base("pager:", PF_HANDLER_STACK_SIZE),
  _badge(reinterpret_cast<unsigned long>(_context->name + 6))
{
	class Create_exception_pt_failed { };
	uint8_t res;

	/* construct pager name out of client name */
	strncpy(_context->name + 6, reinterpret_cast<char const *>(badge),
	        sizeof(_context->name) - 6);

	addr_t pd_sel        = __core_pd_sel;
	_pt_cleanup          = cap_map()->insert(1);
	_client_exc_pt_sel   = cap_map()->insert(NUM_INITIAL_PT_LOG2);
	_state._status       = 0;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;

	if (_pt_cleanup == Native_thread::INVALID_INDEX ||
	    _client_exc_pt_sel == Native_thread::INVALID_INDEX)
		throw Create_exception_pt_failed();

	/* tell thread starting code on which CPU to let run the pager */
	reinterpret_cast<Affinity::Location *>(stack_top())[-1] = location;

	/* creates local EC */
	Thread_base::start();

	/* create portal for exception handlers 0x0 - 0xd */
	for (unsigned i = 0; i < PT_SEL_PAGE_FAULT; i++) {
		res = create_portal(exc_pt_sel_client() + i, pd_sel, _tid.ec_sel,
		                    Mtd(Mtd::EIP), (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
	}

	/* create portal for page-fault handler */
	res = create_portal(exc_pt_sel_client() + PT_SEL_PAGE_FAULT, pd_sel, _tid.ec_sel,
	                    Mtd(Mtd::QUAL | Mtd::EIP), (mword_t)_page_fault_handler);
	if (res) {
		PERR("could not create page-fault portal, error = %u\n", res);
		class Create_page_fault_pt_failed { };
		throw Create_page_fault_pt_failed();
	}

	/* create portal for exception handlers 0xf - 0x19 */
	for (unsigned i = PT_SEL_PAGE_FAULT + 1; i < PT_SEL_PARENT; i++) {
		res = create_portal(exc_pt_sel_client() + i, pd_sel, _tid.ec_sel,
		                    Mtd(Mtd::EIP), (addr_t)_exception_handler);
		if (res) {
			PERR("could not create exception portal, error = %u\n", res);
			throw Create_exception_pt_failed();
		}
	}

	/* create portal for startup handler */
	res = create_portal(exc_pt_sel_client() + PT_SEL_STARTUP, pd_sel, _tid.ec_sel,
	                    Mtd(Mtd::ESP | Mtd::EIP), (mword_t)_startup_handler);
	if (res) {
		PERR("could not create startup portal, error = %u\n",
		     res);
		class Create_startup_pt_failed { };
		throw Create_startup_pt_failed();
	}

	/* create portal for recall handler */
	Mtd mtd(Mtd::ESP | Mtd::EIP | Mtd::ACDB | Mtd::EFL | Mtd::EBSD | Mtd::FSGS);
	res = create_portal(exc_pt_sel_client() + PT_SEL_RECALL, pd_sel, _tid.ec_sel,
	                    mtd, (addr_t)_recall_handler);
	if (res) {
		PERR("could not create recall portal, error = %u\n", res);
		class Create_recall_pt_failed { };
		throw Create_recall_pt_failed();
	}

	/*
	 * Create semaphore required for Genode locking. It can be later on
	 * requested by the thread the same way as all exception portals.
	 */
	res = Nova::create_sm(exc_pt_sel_client() + SM_SEL_EC, pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		class Create_state_notifiy_sm_failed { };
		throw Create_state_notifiy_sm_failed();
	}

	/* create portal for final cleanup call used during destruction */
	res = create_portal(_pt_cleanup, pd_sel, _tid.ec_sel, Mtd(0),
	                    reinterpret_cast<addr_t>(_invoke_handler));
	if (res) {
		PERR("could not create pager cleanup portal, error = %u\n", res);
		class Create_cleanup_pt_failed { };
		throw Create_cleanup_pt_failed();
	}

	res = Nova::create_sm(sm_state_notify(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		class Create_state_notifiy_sm_failed { };
		throw Create_state_notifiy_sm_failed();
	}
}


Pager_object::~Pager_object()
{
	/* if pager is blocked wake him up */
	sm_ctrl(sm_state_notify(), SEMAPHORE_UP);
	revoke(Obj_crd(sm_state_notify(), 0));

	/* take care nobody is handled anymore by this object */
	cleanup_call();

	/* revoke portal used for the cleanup call */
	revoke(Obj_crd(_pt_cleanup, 0));

	cap_map()->remove(_pt_cleanup, 1, false);
	cap_map()->remove(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2, false);
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

