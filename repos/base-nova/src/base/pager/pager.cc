/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>
#include <base/sleep.h>

#include <util/construct_at.h>

#include <rm_session/rm_session.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova_util.h> /* map_local */

using namespace Genode;
using namespace Nova;

extern Genode::addr_t __core_pd_sel;

static Nova::Hip * kernel_hip()
{
	/**
	 * Initial value of esp register, saved by the crt0 startup code.
	 * This value contains the address of the hypervisor information page.
	 */
	extern addr_t __initial_sp;
	return reinterpret_cast<Hip *>(__initial_sp);
}

/* pager activation threads storage and handling - one thread per CPU */
enum { PAGER_CPUS = 128, PAGER_STACK_SIZE = 2*4096 };

static char pager_activation_mem[sizeof (Pager_activation<PAGER_STACK_SIZE>) * PAGER_CPUS];
static Pager_activation_base * pager_threads[PAGER_CPUS];


static unsigned which_cpu(Pager_activation_base * pager)
{
	Pager_activation_base * start = reinterpret_cast<Pager_activation_base *>(&pager_activation_mem);
	Pager_activation_base * end   = start + PAGER_CPUS;

	if (start <= pager && pager < end) {
		/* pager of one of the non boot CPUs */
		unsigned cpu_id = pager - start;
		return cpu_id;
	}

	/* pager of boot CPU */
	return Affinity::Location().xpos();
}


void Pager_object::_page_fault_handler(addr_t pager_obj)
{
	Ipc_pager ipc_pager;
	ipc_pager.wait_for_fault();

	Thread_base  * myself = Thread_base::myself();
	Pager_object *    obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *   utcb = reinterpret_cast<Utcb *>(myself->utcb());
	Pager_activation_base * pager_thread = static_cast<Pager_activation_base *>(myself);

	/* lookup fault address and decide what to do */
	int ret = obj->pager(ipc_pager);

	/* don't open receive window for pager threads */
	if (utcb->crd_rcv.value())
		nova_die();

	/* good case - found a valid region which is mappable */
	if (!ret)
		ipc_pager.reply_and_wait_for_fault();

	obj->_state.thread.ip     = ipc_pager.fault_ip();
	obj->_state.thread.sp     = 0;
	obj->_state.thread.trapno = PT_SEL_PAGE_FAULT;

	obj->_state.block();

	char const * client = reinterpret_cast<char const *>(obj->_badge);
	/* region manager fault - to be handled */
	if (ret == 1) {
		PDBG("page fault, thread '%s', cpu %u, ip=%lx, fault address=0x%lx",
		     client, which_cpu(pager_thread), ipc_pager.fault_ip(),
		     ipc_pager.fault_addr());

		utcb->set_msg_word(0);
		utcb->mtd = 0;

		/* block the faulting thread until region manager is done */
		ipc_pager.reply_and_wait_for_fault(obj->sel_sm_block());
	}

	/* unhandled case */
	obj->_state.mark_dead();

	PWRN("unresolvable page fault, thread '%s', cpu %u, ip=%lx, "
	     "fault address=0x%lx ret=%u", client, which_cpu(pager_thread),
	     ipc_pager.fault_ip(), ipc_pager.fault_addr(), ret);

	Native_capability pager_cap = obj->Object_pool<Pager_object>::Entry::cap();
	revoke(pager_cap.dst());

	revoke(Obj_crd(obj->exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

	utcb->set_msg_word(0);
	utcb->mtd = 0;
	ipc_pager.reply_and_wait_for_fault();
}


void Pager_object::exception(uint8_t exit_id)
{
	Thread_base  *myself = Thread_base::myself();
	Utcb         *  utcb = reinterpret_cast<Utcb *>(myself->utcb());
	Pager_activation_base * pager_thread = static_cast<Pager_activation_base *>(myself);

	if (exit_id > PT_SEL_PARENT || !pager_thread)
		nova_die();

	addr_t fault_ip    = utcb->ip;
	uint8_t res        = 0xFF;
	addr_t  mtd        = 0;

	if (_state.skip_requested()) {
		_state.skip_reset();

		utcb->set_msg_word(0);
		utcb->mtd = 0;
		reply(myself->stack_top());
	}

	/* remember exception type for cpu_session()->state() calls */
	_state.thread.trapno = exit_id;
	_state.thread.ip     = fault_ip;

	if (_exception_sigh.valid()) {
		_state.submit_signal();
		res = client_recall();
	}

	if (res != NOVA_OK) {
		/* nobody handles this exception - so thread will be stopped finally */
		_state.mark_dead();

		char const * client = reinterpret_cast<char const *>(_badge);
		PWRN("unresolvable exception %u, thread '%s', cpu %u, ip=0x%lx, %s",
		     exit_id, client, which_cpu(pager_thread), fault_ip,
		     res == 0xFF ? "no signal handler" :
		     (res == NOVA_OK ? "" : "recall failed"));

		Nova::revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

		enum { TRAP_BREAKPOINT = 3 };

		if (exit_id == TRAP_BREAKPOINT) {
			utcb->ip = fault_ip - 1;
			mtd      = Mtd::EIP;
		}
	}

	utcb->set_msg_word(0);
	utcb->mtd = mtd;

	reply(myself->stack_top());
}


void Pager_object::_recall_handler(addr_t pager_obj)
{
	Thread_base  * myself = Thread_base::myself();
	Pager_object *    obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *   utcb = reinterpret_cast<Utcb *>(myself->utcb());

	/* save state - can be requested via cpu_session->state */
	obj->_copy_state(utcb);

	obj->_state.thread.ip     = utcb->ip;
	obj->_state.thread.sp     = utcb->sp;

	obj->_state.thread.eflags = utcb->flags;

	/* thread becomes blocked */
	obj->_state.block();

	/* deliver signal if it was requested */
	if (obj->_state.to_submit())
		obj->submit_exception_signal();

	/* notify callers of cpu_session()->pause that the state is now valid */
	if (obj->_state.notify_requested()) {
		obj->_state.notify_cancel();
		if (sm_ctrl(obj->sel_sm_notify(), SEMAPHORE_UP) != NOVA_OK)
			PWRN("paused notification failed");
	}

	/* switch on/off single step */
	bool singlestep_state = obj->_state.thread.eflags & 0x100UL;
	if (obj->_state.singlestep() && !singlestep_state) {
		utcb->flags = obj->_state.thread.eflags | 0x100UL;
		utcb->mtd = Nova::Mtd(Mtd::EFL).value();
	} else {
		if (!obj->_state.singlestep() && singlestep_state) {
			utcb->flags = obj->_state.thread.eflags & ~0x100UL;
			utcb->mtd = Nova::Mtd(Mtd::EFL).value();
		} else
			utcb->mtd = 0;
	}

	/* block until cpu_session()->resume() respectively wake_up() call */
	utcb->set_msg_word(0);
	reply(myself->stack_top(), obj->sel_sm_block());
}


void Pager_object::_startup_handler(addr_t pager_obj)
{
	Thread_base  *myself = Thread_base::myself();
	Pager_object *   obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *  utcb = reinterpret_cast<Utcb *>(myself->utcb());

	utcb->ip  = obj->_initial_eip;
	utcb->sp  = obj->_initial_esp;

	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);

	reply(myself->stack_top());
}


void Pager_object::_invoke_handler(addr_t pager_obj)
{
	Thread_base  *myself = Thread_base::myself();
	Pager_object *   obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *  utcb = reinterpret_cast<Utcb *>(myself->utcb());

	/* receive window must be closed - otherwise implementation bug */
	if (utcb->crd_rcv.value())
		nova_die();

	addr_t const event    = utcb->msg[0];
	addr_t const logcount = utcb->msg[1];

	/* check for translated vCPU portals */
	unsigned const items_count = 1U << (Nova::NUM_INITIAL_VCPU_PT_LOG2 - 1);

	if ((obj->_client_exc_vcpu != Native_thread::INVALID_INDEX) &&
	    (utcb->msg_items() == items_count) &&
	    (utcb->msg_words() == 1 && (event == 0UL || event == 1UL))) {
		/* check all translated item and remap if valid */
		for (unsigned i = 0; i < items_count; i++) {
			Nova::Utcb::Item * item = utcb->get_item(i);

			if (!item)
				break;

			Nova::Crd cap(item->crd);

			if (cap.is_null() || item->is_del())
				continue;

			/**
			 * Remap portal to dense packed region - required for vCPU running
			 * in separate PD (non-colocated case)
			 */
			Obj_crd snd(cap.base(), 0);
			Obj_crd rcv(obj->_client_exc_vcpu + event * items_count + i, 0);
			if (map_local(utcb, snd, rcv))
				PWRN("could not remap vCPU portal 0x%x", i);
		}
	}

	/* if protocol is violated ignore request */
	if (utcb->msg_words() != 2) {
		utcb->mtd = 0;
		utcb->set_msg_word(0);
		reply(myself->stack_top());
	}

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
		/* if logcount > 0 then the pager cap should also be mapped */
		if (logcount)
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
	if (!_state.blocked())
		return;

	_state.unblock();

	uint8_t res = sm_ctrl(sel_sm_block(), SEMAPHORE_UP);
	if (res != NOVA_OK)
		PWRN("canceling blocked client failed (thread sm)");
}


void Pager_object::client_cancel_blocking()
{
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
	return ec_ctrl(EC_RECALL, _state.sel_client_ec);
}


void Pager_object::cleanup_call()
{
	_state.mark_dissolved();

	/* revoke all portals handling the client. */
	revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

	/* if we are paused or waiting for a page fault nothing is in-flight */
	if (_state.blocked())
		return;

	Utcb *utcb = reinterpret_cast<Utcb *>(Thread_base::myself()->utcb());
	utcb->set_msg_word(0);
	utcb->mtd = 0;
	if (uint8_t res = call(sel_pt_cleanup()))
		PERR("%8p - cleanup call to pager failed res=%d", utcb, res);
}


static uint8_t create_portal(addr_t pt, addr_t pd, addr_t ec, Mtd mtd,
                             addr_t eip, addr_t localname)
{
	uint8_t res = create_pt(pt, pd, ec, mtd, eip);

	if (res != NOVA_OK)
		return res;

	res = pt_ctrl(pt, localname);
	if (res == NOVA_OK)
		revoke(Obj_crd(pt, 0, Obj_crd::RIGHT_PT_CTRL));
	else
		revoke(Obj_crd(pt, 0));

	return res;
}


/************************
 ** Exception handlers **
 ************************/

template <uint8_t EV>
void Exception_handlers::register_handler(Pager_object *obj, Mtd mtd,
                                          void (* __attribute__((regparm(1))) func)(addr_t))
{
	unsigned use_cpu = obj->location.xpos();
	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu])
		throw Rm_session::Invalid_thread();

	addr_t const ec_sel = pager_threads[use_cpu]->tid().ec_sel;

	/* compiler generates instance of exception entry if not specified */
	addr_t entry = func ? (addr_t)func : (addr_t)(&_handler<EV>);
	uint8_t res = create_portal(obj->exc_pt_sel_client() + EV,
	                            __core_pd_sel, ec_sel, mtd, entry,
	                            reinterpret_cast<addr_t>(obj));
	if (res != Nova::NOVA_OK)
		throw Rm_session::Invalid_thread();
}


template <uint8_t EV>
void Exception_handlers::_handler(addr_t obj)
{
	Pager_object * pager_obj = reinterpret_cast<Pager_object *>(obj);
	pager_obj->exception(EV);
}


Exception_handlers::Exception_handlers(Pager_object *obj)
{
	register_handler<0>(obj, Mtd(Mtd::EIP));
	register_handler<1>(obj, Mtd(Mtd::EIP));
	register_handler<2>(obj, Mtd(Mtd::EIP));
	register_handler<3>(obj, Mtd(Mtd::EIP));
	register_handler<4>(obj, Mtd(Mtd::EIP));
	register_handler<5>(obj, Mtd(Mtd::EIP));
	register_handler<6>(obj, Mtd(Mtd::EIP));
	register_handler<7>(obj, Mtd(Mtd::EIP));
	register_handler<8>(obj, Mtd(Mtd::EIP));
	register_handler<9>(obj, Mtd(Mtd::EIP));
	register_handler<10>(obj, Mtd(Mtd::EIP));
	register_handler<11>(obj, Mtd(Mtd::EIP));
	register_handler<12>(obj, Mtd(Mtd::EIP));
	register_handler<13>(obj, Mtd(Mtd::EIP));

	register_handler<15>(obj, Mtd(Mtd::EIP));
	register_handler<16>(obj, Mtd(Mtd::EIP));
	register_handler<17>(obj, Mtd(Mtd::EIP));
	register_handler<18>(obj, Mtd(Mtd::EIP));
	register_handler<19>(obj, Mtd(Mtd::EIP));
	register_handler<20>(obj, Mtd(Mtd::EIP));
	register_handler<21>(obj, Mtd(Mtd::EIP));
	register_handler<22>(obj, Mtd(Mtd::EIP));
	register_handler<23>(obj, Mtd(Mtd::EIP));
	register_handler<24>(obj, Mtd(Mtd::EIP));
	register_handler<25>(obj, Mtd(Mtd::EIP));
}


/******************
 ** Pager object **
 ******************/


Pager_object::Pager_object(unsigned long badge, Affinity::Location location)
:
	_badge(badge),
	_selectors(cap_map()->insert(2)),
	_client_exc_pt_sel(cap_map()->insert(NUM_INITIAL_PT_LOG2)),
	_client_exc_vcpu(Native_thread::INVALID_INDEX),
	_exceptions(this),
	location(location)
{
	uint8_t res;

	addr_t pd_sel        = __core_pd_sel;
	_state._status       = 0;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;

	if (Native_thread::INVALID_INDEX == _selectors ||
	    Native_thread::INVALID_INDEX == _client_exc_pt_sel)
		throw Rm_session::Invalid_thread();

	/* ypos information not supported by now */
	if (location.ypos()) {
		PWRN("Unsupported location %ux%u", location.xpos(), location.ypos());
		throw Rm_session::Invalid_thread();
	}

	/* place Pager_object on specified CPU by selecting proper pager thread */
	unsigned use_cpu = location.xpos();
	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu])
		throw Rm_session::Invalid_thread();

	addr_t ec_sel    = pager_threads[use_cpu]->tid().ec_sel;

	/* create portal for page-fault handler - 14 */
	_exceptions.register_handler<14>(this, Mtd::QUAL | Mtd::EIP,
	                                 _page_fault_handler);

	/* create portal for startup handler - 26 */
	Mtd const mtd_startup(Mtd::ESP | Mtd::EIP);
	_exceptions.register_handler<PT_SEL_STARTUP>(this, mtd_startup,
	                                             _startup_handler);

	/* create portal for recall handler - 31 */
	Mtd const mtd_recall(Mtd::ESP | Mtd::EIP | Mtd::ACDB | Mtd::EFL |
	                     Mtd::EBSD | Mtd::FSGS);
	_exceptions.register_handler<PT_SEL_RECALL>(this, mtd_recall,
	                                            _recall_handler);

	/*
	 * Create semaphore required for Genode locking. It can be later on
	 * requested by the thread the same way as all exception portals.
	 */
	res = Nova::create_sm(exc_pt_sel_client() + SM_SEL_EC, pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Rm_session::Invalid_thread();
	}

	/* create portal for final cleanup call used during destruction */
	res = create_portal(sel_pt_cleanup(), pd_sel, ec_sel, Mtd(0),
	                    reinterpret_cast<addr_t>(_invoke_handler),
	                    reinterpret_cast<addr_t>(this));
	if (res != Nova::NOVA_OK) {
		PERR("could not create pager cleanup portal, error = %u\n", res);
		throw Rm_session::Invalid_thread();
	}

	/* used to notify caller of as soon as pause succeeded */
	res = Nova::create_sm(sel_sm_notify(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Rm_session::Invalid_thread();
	}

	/* semaphore used to block paged thread during page fault or recall */
	res = Nova::create_sm(sel_sm_block(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Rm_session::Invalid_thread();
	}
}


Pager_object::~Pager_object()
{
	/* sanity check that object got dissolved already - otherwise bug */
	if (!_state.dissolved())
		nova_die();

	/* revoke portal used for the cleanup call and sm cap for blocking state */
	revoke(Obj_crd(_selectors, 2));
	cap_map()->remove(_selectors, 2, false);
	cap_map()->remove(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2, false);

	if (_client_exc_vcpu == Native_thread::INVALID_INDEX)
		return;

	/* revoke vCPU exception portals */
	revoke(Obj_crd(_client_exc_vcpu, NUM_INITIAL_VCPU_PT_LOG2));
	cap_map()->remove(_client_exc_vcpu, NUM_INITIAL_VCPU_PT_LOG2, false);
}


/**********************
 ** Pager activation **
 **********************/

Pager_activation_base::Pager_activation_base(const char *name, size_t stack_size)
:
	Thread_base(Cpu_session::DEFAULT_WEIGHT, name, stack_size),
	_cap(Native_capability()), _ep(0), _cap_valid(Lock::LOCKED)
{
	/* tell thread starting code on which CPU to let run the pager */
	reinterpret_cast<Affinity::Location *>(stack_base())[0] = Affinity::Location(which_cpu(this), 0);

	/* creates local EC */
	Thread_base::start();

	reinterpret_cast<Nova::Utcb *>(Thread_base::utcb())->crd_xlt = Obj_crd(0, ~0UL);
}


void Pager_activation_base::entry() { }


/**********************
 ** Pager entrypoint **
 **********************/


Pager_entrypoint::Pager_entrypoint(Cap_session           *cap_session,
                                   Pager_activation_base *a)
: _activation(a), _cap_session(cap_session)
{
	/* sanity check space for pager threads */
	if (kernel_hip()->cpu_max() > PAGER_CPUS) {
		PERR("kernel supports more CPUs (%u) than Genode (%u)",
		     kernel_hip()->cpu_max(), PAGER_CPUS);
		nova_die();
	}

	/* determine boot cpu */
	unsigned master_cpu = boot_cpu();

	/* detect enabled CPUs and create per CPU a pager thread */
	typedef Pager_activation<PAGER_STACK_SIZE> Pager;
	Pager * pager_of_cpu = reinterpret_cast<Pager *>(&pager_activation_mem);

	for (unsigned i = 0; i < kernel_hip()->cpu_max(); i++, pager_of_cpu++) {
		if (i == master_cpu) {
			pager_threads[master_cpu] = a;
			a->ep(this);
			continue;
		}

		if (!kernel_hip()->is_cpu_enabled(i))
			continue;

		pager_threads[i] = pager_of_cpu;
		construct_at<Pager>(pager_threads[i]);
		pager_threads[i]->ep(this);
	}
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* let handle pager_object of pager thread on same CPU */
	unsigned use_cpu = obj->location.xpos();
	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu]) {
		PWRN("invalid CPU parameter used in pager object");
		return Pager_capability();
	}
	Native_capability pager_thread_cap(pager_threads[use_cpu]->tid().ec_sel);

	/* request creation of portal bind to pager thread */
	Native_capability cap_session =
		_cap_session->alloc(pager_thread_cap, obj->handler_address());

	if (NOVA_OK != pt_ctrl(cap_session.local_name(), reinterpret_cast<mword_t>(obj)))
		nova_die();

	/* disable the feature for security reasons now */
	revoke(Obj_crd(cap_session.local_name(), 0, Obj_crd::RIGHT_PT_CTRL));

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
	/* revoke cap selector locally */
	revoke(pager_obj.dst(), true);
	/* remove object from pool */
	remove_locked(obj);
	/* take care that no faults are in-flight */
	obj->cleanup_call();
}
