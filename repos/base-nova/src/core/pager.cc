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
#include <base/sleep.h>
#include <util/construct_at.h>
#include <rm_session/rm_session.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* core-local includes */
#include <pager.h>
#include <platform_thread.h>
#include <imprint_badge.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova_util.h> /* map_local */
#include <nova/capability_space.h>

static bool verbose_oom = false;

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


/**
 * Utility for the formatted output of page-fault information
 */
struct Page_fault_info
{
	char const * const pd;
	char const * const thread;
	unsigned const cpu;
	addr_t const ip, addr;

	Page_fault_info(char const *pd, char const *thread, unsigned cpu,
	                addr_t ip, addr_t addr)
	: pd(pd), thread(thread), cpu(cpu), ip(ip), addr(addr) { }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "pd='",     pd,      "' "
		                   "thread='", thread,  "' "
		                   "cpu=",     cpu,     " "
		                   "ip=",      Hex(ip), " "
		                   "address=", Hex(addr));
	}
};


void Pager_object::_page_fault_handler(addr_t pager_obj)
{
	Ipc_pager ipc_pager;
	ipc_pager.wait_for_fault();

	Thread       * myself = Thread::myself();
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

	obj->_state_lock.lock();

	obj->_state.thread.ip     = ipc_pager.fault_ip();
	obj->_state.thread.sp     = 0;
	obj->_state.thread.trapno = PT_SEL_PAGE_FAULT;

	obj->_state.block();

	obj->_state_lock.unlock();

	const char * client_thread = obj->client_thread();
	const char * client_pd = obj->client_pd();

	Page_fault_info const fault_info(client_pd, client_thread,
	                                 which_cpu(pager_thread),
	                                 ipc_pager.fault_ip(), ipc_pager.fault_addr());

	/* region manager fault - to be handled */
	if (ret == 1) {
		log("page fault, ", fault_info);

		utcb->set_msg_word(0);
		utcb->mtd = 0;

		/* block the faulting thread until region manager is done */
		ipc_pager.reply_and_wait_for_fault(obj->sel_sm_block_pause());
	}

	/* unhandled case */
	obj->_state.mark_dead();

	warning("unresolvable page fault, ", fault_info, " ret=", ret);

	Native_capability pager_cap = obj->Object_pool<Pager_object>::Entry::cap();

	revoke(Capability_space::crd(pager_cap).base());

	revoke(Obj_crd(obj->exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

	utcb->set_msg_word(0);
	utcb->mtd = 0;
	ipc_pager.reply_and_wait_for_fault();
}


void Pager_object::exception(uint8_t exit_id)
{
	Thread       *myself = Thread::myself();
	Utcb         *  utcb = reinterpret_cast<Utcb *>(myself->utcb());
	Pager_activation_base * pager_thread = static_cast<Pager_activation_base *>(myself);

	if (exit_id > PT_SEL_PARENT || !pager_thread)
		nova_die();

	addr_t fault_ip    = utcb->ip;
	uint8_t res        = 0xFF;
	addr_t  mtd        = 0;

	_state_lock.lock();

	/* remember exception type for cpu_session()->state() calls */
	_state.thread.trapno = exit_id;

	if (_exception_sigh.valid()) {
		_state.submit_signal();
		res = _unsynchronized_client_recall(true);
	}

	if (res != NOVA_OK) {
		/* nobody handles this exception - so thread will be stopped finally */
		_state.mark_dead();

		warning("unresolvable exception ", exit_id,  ", "
		        "pd '",     client_pd(),            "', "
		        "thread '", client_thread(),        "', "
		        "cpu ",     which_cpu(pager_thread), ", "
		        "ip=",      Hex(fault_ip),            " ",
		        res == 0xFF ? "no signal handler"
		                    : (res == NOVA_OK ? "" : "recall failed"));

		Nova::revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

		enum { TRAP_BREAKPOINT = 3 };

		if (exit_id == TRAP_BREAKPOINT) {
			utcb->ip = fault_ip - 1;
			mtd      = Mtd::EIP;
		}
	}

	_state_lock.unlock();

	utcb->set_msg_word(0);
	utcb->mtd = mtd;

	reply(myself->stack_top());
}


void Pager_object::_recall_handler(addr_t pager_obj)
{
	Thread       * myself = Thread::myself();
	Pager_object *    obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *   utcb = reinterpret_cast<Utcb *>(myself->utcb());

	obj->_state_lock.lock();

	if (obj->_state.modified) {
		obj->_copy_state_to_utcb(utcb);
		obj->_state.modified = false;
	} else
		utcb->mtd = 0;

	/* switch on/off single step */
	bool singlestep_state = obj->_state.thread.eflags & 0x100UL;
	if (obj->_state.singlestep() && !singlestep_state) {
		utcb->flags |= 0x100UL;
		utcb->mtd |= Mtd::EFL;
	} else if (!obj->_state.singlestep() && singlestep_state) {
		utcb->flags &= ~0x100UL;
		utcb->mtd |= Mtd::EFL;
	}

	/* deliver signal if it was requested */
	if (obj->_state.to_submit())
		obj->submit_exception_signal();

	/* block until cpu_session()->resume() respectively wake_up() call */

	unsigned long sm = obj->_state.blocked() ? obj->sel_sm_block_pause() : 0;

	obj->_state_lock.unlock();

	utcb->set_msg_word(0);
	reply(myself->stack_top(), sm);
}


void Pager_object::_startup_handler(addr_t pager_obj)
{
	Thread       *myself = Thread::myself();
	Pager_object *   obj = reinterpret_cast<Pager_object *>(pager_obj);
	Utcb         *  utcb = reinterpret_cast<Utcb *>(myself->utcb());

	utcb->ip  = obj->_initial_eip;
	utcb->sp  = obj->_initial_esp;

	utcb->mtd = Mtd::EIP | Mtd::ESP;

	if (obj->_state.singlestep()) {
		utcb->flags = 0x100UL;
		utcb->mtd |= Mtd::EFL;
	}

	obj->_state.unblock();

	utcb->set_msg_word(0);

	reply(myself->stack_top());
}


void Pager_object::_invoke_handler(addr_t pager_obj)
{
	Thread       *myself = Thread::myself();
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
				warning("could not remap vCPU portal ", Hex(i));
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
	Lock::Guard _state_lock_guard(_state_lock);

	if (!_state.blocked())
		return;

	_state.thread.exception = false;

	_state.unblock();

	uint8_t res = sm_ctrl(sel_sm_block_pause(), SEMAPHORE_UP);
	if (res != NOVA_OK)
		warning("canceling blocked client failed (thread sm)");
}


void Pager_object::client_cancel_blocking()
{
	uint8_t res = sm_ctrl(exc_pt_sel_client() + SM_SEL_EC, SEMAPHORE_UP);
	if (res != NOVA_OK)
		warning("canceling blocked client failed (thread sm)");

	if (!_state.has_signal_sm())
		return;

	res = sm_ctrl(exc_pt_sel_client() + PT_SEL_STARTUP, SEMAPHORE_UP);
	if (res != NOVA_OK)
		warning("canceling blocked client failed (signal sm)");
}


uint8_t Pager_object::client_recall(bool get_state_and_block)
{
	Lock::Guard _state_lock_guard(_state_lock);
	return _unsynchronized_client_recall(get_state_and_block);
}


uint8_t Pager_object::_unsynchronized_client_recall(bool get_state_and_block)
{
	enum { STATE_REQUESTED = 1 };

	uint8_t res = ec_ctrl(EC_RECALL, _state.sel_client_ec,
	                      get_state_and_block ? STATE_REQUESTED : ~0UL);

	if (res != NOVA_OK)
		return res;

	if (get_state_and_block) {
		Utcb *utcb = reinterpret_cast<Utcb *>(Thread::myself()->utcb());
		_copy_state_from_utcb(utcb);
		_state.block();
	}

	return res;
}


void Pager_object::cleanup_call()
{
	_state.mark_dissolved();

	/* revoke ec and sc cap of client before the sm cap */
	if (_state.sel_client_ec != Native_thread::INVALID_INDEX)
		revoke(Obj_crd(_state.sel_client_ec, 2));

	/* revoke all portals handling the client. */
	revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

	Utcb *utcb = reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	utcb->set_msg_word(0);
	utcb->mtd = 0;
	if (uint8_t res = call(sel_pt_cleanup()))
		error(utcb, " - cleanup call to pager failed res=", res);
}


void Pager_object::print(Output &out) const
{
	Platform_thread * faulter = reinterpret_cast<Platform_thread *>(_badge);
	Genode::print(out, "pager_object: pd='",
			faulter ? faulter->pd_name() : "unknown", "' thread='",
			faulter ? faulter->name() : "unknown", "'");
}


static uint8_t create_portal(addr_t pt, addr_t pd, addr_t ec, Mtd mtd,
                             addr_t eip, Pager_object * oom_handler)
{
	addr_t const badge_localname = reinterpret_cast<addr_t>(oom_handler);

	uint8_t res;
	do {
		res = create_pt(pt, pd, ec, mtd, eip);
	} while (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK == oom_handler->handle_oom());

	if (res != NOVA_OK)
		return res;

	res = pt_ctrl(pt, badge_localname);
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
	unsigned use_cpu = obj->location().xpos();

	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu]) {
		warning("invalid CPU parameter used in pager object");
		throw Region_map::Invalid_thread();
	}

	addr_t const ec_sel = pager_threads[use_cpu]->native_thread().ec_sel;

	/* compiler generates instance of exception entry if not specified */
	addr_t entry = func ? (addr_t)func : (addr_t)(&_handler<EV>);
	uint8_t res = create_portal(obj->exc_pt_sel_client() + EV,
	                            __core_pd_sel, ec_sel, mtd, entry, obj);
	if (res != Nova::NOVA_OK)
		throw Region_map::Invalid_thread();
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


Pager_object::Pager_object(Cpu_session_capability cpu_session_cap,
                           Thread_capability thread_cap, unsigned long badge,
                           Affinity::Location location, Session_label const &,
                           Cpu_session::Name const &)
:
	_badge(badge),
	_selectors(cap_map()->insert(2)),
	_client_exc_pt_sel(cap_map()->insert(NUM_INITIAL_PT_LOG2)),
	_client_exc_vcpu(Native_thread::INVALID_INDEX),
	_cpu_session_cap(cpu_session_cap), _thread_cap(thread_cap),
	_location(location),
	_exceptions(this)
{
	uint8_t res;

	addr_t pd_sel        = __core_pd_sel;
	_state._status       = 0;
	_state.modified      = false;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;
	_state.block();

	if (Native_thread::INVALID_INDEX == _selectors ||
	    Native_thread::INVALID_INDEX == _client_exc_pt_sel)
		throw Region_map::Invalid_thread();

	/* ypos information not supported by now */
	if (location.ypos()) {
		warning("unsupported location ", location.xpos(), "x", location.ypos());
		throw Region_map::Invalid_thread();
	}

	/* place Pager_object on specified CPU by selecting proper pager thread */
	unsigned use_cpu = location.xpos();
	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu]) {
		warning("invalid CPU parameter used in pager object");
		throw Region_map::Invalid_thread();
	}

	addr_t ec_sel    = pager_threads[use_cpu]->native_thread().ec_sel;

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
		throw Region_map::Invalid_thread();
	}

	/* create portal for final cleanup call used during destruction */
	res = create_portal(sel_pt_cleanup(), pd_sel, ec_sel, Mtd(0),
	                    reinterpret_cast<addr_t>(_invoke_handler), this);
	if (res != Nova::NOVA_OK) {
		error("could not create pager cleanup portal, error=", res);
		throw Region_map::Invalid_thread();
	}

	/* semaphore used to block paged thread during recall */
	res = Nova::create_sm(sel_sm_block_pause(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Region_map::Invalid_thread();
	}

	/* semaphore used to block paged thread during OOM memory revoke */
	res = Nova::create_sm(sel_sm_block_oom(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Region_map::Invalid_thread();
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

uint8_t Pager_object::handle_oom(addr_t transfer_from,
                                 char const * src_pd, char const * src_thread,
                                 enum Pager_object::Policy policy)
{
	const char * dst_pd     = client_pd();
	const char * dst_thread = client_thread();

	enum { QUOTA_TRANSFER_PAGES = 2 };

	if (transfer_from == SRC_CORE_PD)
		transfer_from = __core_pd_sel;

	/* request current kernel quota usage of target pd */
	addr_t limit_before = 0, usage_before = 0;
	Nova::pd_ctrl_debug(pd_sel(), limit_before, usage_before);

	if (verbose_oom) {
		addr_t limit_source = 0, usage_source = 0;
		/* request current kernel quota usage of source pd */
		Nova::pd_ctrl_debug(transfer_from, limit_source, usage_source);

		log("oom - '", dst_pd, "':'", dst_thread, "' "
		    "(", usage_before, "/", limit_before, ") - "
		    "transfer ", (long)QUOTA_TRANSFER_PAGES, " pages "
		    "from '", src_pd, "':'", src_thread, "' "
		    "(", usage_source, "/", limit_source, ")");
	}

	uint8_t res = Nova::NOVA_PD_OOM;

	if (transfer_from != pd_sel()) {
		/* upgrade quota */
		uint8_t res = Nova::pd_ctrl(transfer_from, Pd_op::TRANSFER_QUOTA,
		                            pd_sel(), QUOTA_TRANSFER_PAGES);
		if (res == Nova::NOVA_OK)
			return res;
	}

	/* retry upgrade using core quota if policy permits */
	if (policy == UPGRADE_PREFER_SRC_TO_DST) {
		if (transfer_from != __core_pd_sel) {
			res = Nova::pd_ctrl(__core_pd_sel, Pd_op::TRANSFER_QUOTA,
			                    pd_sel(), QUOTA_TRANSFER_PAGES);
			if (res == Nova::NOVA_OK)
				return res;
		}
	}

	warning("kernel memory quota upgrade failed - trigger memory free up for "
	        "causing '", dst_pd, "':'", dst_thread, "' - "
	        "donator is '", src_pd, "':'", src_thread, "', "
	        "policy=", (int)policy);

	/* if nothing helps try to revoke memory */
	enum { REMOTE_REVOKE = true, PD_SELF = true };
	Mem_crd crd_all(0, ~0U, Rights(true, true, true));
	Nova::revoke(crd_all, PD_SELF, REMOTE_REVOKE, pd_sel(), sel_sm_block_oom());

	/* re-request current kernel quota usage of target pd */
	addr_t limit_after = 0, usage_after = 0;
	Nova::pd_ctrl_debug(pd_sel(), limit_after, usage_after);
	/* if we could free up memory we continue */
	if (usage_after < usage_before)
		return Nova::NOVA_OK;

	/*
	 * There is still the chance that memory gets freed up, but one has to
	 * wait until RCU period is over. If we are in the pager code, we can
	 * instruct the kernel to block the faulting client thread during the reply
	 * syscall. If we are in a normal (non-pagefault) RPC service call,
	 * we can't block. The caller of this function can decide based on
	 * the return value what to do and whether blocking is ok.
	 */
	return Nova::NOVA_PD_OOM;
}


void Pager_object::_oom_handler(addr_t pager_dst, addr_t pager_src,
                                addr_t reason)
{
	if (sizeof(void *) == 4) {
		/* On 32 bit edx and ecx as second and third regparm parameter is not
		 * available. It is used by the kernel internally to store ip/sp.
		 */
		asm volatile ("" : "=D" (pager_src));
		asm volatile ("" : "=S" (reason));
	}

	Thread       * myself  = Thread::myself();
	Utcb         * utcb    = reinterpret_cast<Utcb *>(myself->utcb());
	Pager_object * obj_dst = reinterpret_cast<Pager_object *>(pager_dst);
	Pager_object * obj_src = reinterpret_cast<Pager_object *>(pager_src);

	/* Policy used if the Process of the paged thread runs out of memory */
	enum Policy policy = Policy::UPGRADE_CORE_TO_DST;


	/* check assertions - cases that should not happen on Genode@Nova */
	enum { NO_OOM_PT = ~0UL, EC_OF_PT_OOM_OUTSIDE_OF_CORE };

	/* all relevant (user) threads should have a OOM PT */
	bool assert = pager_dst == NO_OOM_PT;

	/*
	 * PT OOM solely created by core and they have to point to the pager
	 * thread inside core.
	 */
	assert |= pager_dst == EC_OF_PT_OOM_OUTSIDE_OF_CORE;

	/*
	 * This pager thread does solely reply to IPC calls - it should never
	 * cause OOM during the sending phase of a IPC.
	 */
	assert |= ((reason & (SELF | SEND)) == (SELF | SEND));

	/*
	 * This pager thread should never send words (untyped items) - it just
	 * answers page faults by typed items (memory mappings).
	 */
	assert |= utcb->msg_words();

	if (assert) {
		error("unknown OOM case - stop core pager thread");
		utcb->set_msg_word(0);
		reply(myself->stack_top(), myself->native_thread().exc_pt_sel + Nova::SM_SEL_EC);
	}

	/* be strict in case of the -strict- STOP policy - stop causing thread */
	if (policy == STOP) {
		error("PD has insufficient kernel memory left - stop thread");
		utcb->set_msg_word(0);
		reply(myself->stack_top(), obj_dst->sel_sm_block_pause());
	}

	char const * src_pd     = "core";
	char const * src_thread = "pager";

	addr_t transfer_from = SRC_CORE_PD;

	switch (pager_src) {
	case SRC_PD_UNKNOWN:
		/* should not happen on Genode - we create and know every PD in core */
		error("Unknown PD has insufficient kernel memory left - stop thread");
		utcb->set_msg_word(0);
		reply(myself->stack_top(), myself->native_thread().exc_pt_sel + Nova::SM_SEL_EC);

	case SRC_CORE_PD:
		/* core PD -> other PD, which has insufficient kernel resources */

		if (!(reason & SELF)) {
			/* case that src thread != this thread in core */
			src_thread = "unknown";
			utcb->set_msg_word(0);
        }

		transfer_from = __core_pd_sel;
		break;
	default:
		/* non core PD -> non core PD */
		utcb->set_msg_word(0);

		if (pager_src == pager_dst || policy == UPGRADE_CORE_TO_DST)
			transfer_from = __core_pd_sel;
		else {
			/* delegation of items between different PDs */
			src_pd = obj_src->client_pd();
			src_thread = obj_src->client_thread();
			transfer_from = obj_src->pd_sel();
		}
	}

	uint8_t res = obj_dst->handle_oom(transfer_from, src_pd, src_thread,
	                                  policy);
	if (res == Nova::NOVA_OK)
		/* handling succeeded - continue with original IPC */
		reply(myself->stack_top());

	/* transfer nothing */
	utcb->set_msg_word(0);

	if (res != Nova::NOVA_PD_OOM)
		error("upgrading kernel memory failed, policy ", (int)policy, ", "
		      "error ", (int)res, " - stop thread finally");

	/* else: caller will get blocked until RCU period is over */

	/* block caller in semaphore */
	reply(myself->stack_top(), obj_dst->sel_sm_block_oom());
}


addr_t Pager_object::get_oom_portal()
{
	addr_t const pt_oom     = sel_oom_portal();
	unsigned const use_cpu  = _location.xpos();

	if (kernel_hip()->is_cpu_enabled(use_cpu) && pager_threads[use_cpu]) {
		addr_t const ec_sel     = pager_threads[use_cpu]->native_thread().ec_sel;

		uint8_t res = create_portal(pt_oom, __core_pd_sel, ec_sel, Mtd(0),
		                            reinterpret_cast<addr_t>(_oom_handler),
		                            this);
		if (res == Nova::NOVA_OK)
			return pt_oom;
	}

	error("creating portal for out of memory notification failed");
	return 0;
}


const char * Pager_object::client_thread() const
{
	Platform_thread * client = reinterpret_cast<Platform_thread *>(_badge);
	return client ? client->name() : "unknown";
}


const char * Pager_object::client_pd() const
{
	Platform_thread * client = reinterpret_cast<Platform_thread *>(_badge);
	return client ? client->pd_name() : "unknown";
}

/**********************
 ** Pager activation **
 **********************/

Pager_activation_base::Pager_activation_base(const char *name, size_t stack_size)
:
	Thread(Cpu_session::Weight::DEFAULT_WEIGHT, name, stack_size,
	       Affinity::Location(which_cpu(this), 0)),
	_cap(Native_capability()), _ep(0), _cap_valid(Lock::LOCKED)
{
	/* creates local EC */
	Thread::start();

	reinterpret_cast<Nova::Utcb *>(Thread::utcb())->crd_xlt = Obj_crd(0, ~0UL);
}


void Pager_activation_base::entry() { }


/**********************
 ** Pager entrypoint **
 **********************/


Pager_entrypoint::Pager_entrypoint(Rpc_cap_factory &cap_factory)
: _cap_factory(cap_factory)
{
	/* sanity check for pager threads */
	if (kernel_hip()->cpu_max() > PAGER_CPUS) {
		error("kernel supports more CPUs (", kernel_hip()->cpu_max(), ") "
		      "than Genode (", (unsigned)PAGER_CPUS, ")");
		nova_die();
	}

	/* detect enabled CPUs and create per CPU a pager thread */
	typedef Pager_activation<PAGER_STACK_SIZE> Pager;
	Pager * pager_of_cpu = reinterpret_cast<Pager *>(&pager_activation_mem);

	for (unsigned i = 0; i < kernel_hip()->cpu_max(); i++, pager_of_cpu++) {
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
	unsigned use_cpu = obj->location().xpos();
	if (!kernel_hip()->is_cpu_enabled(use_cpu) || !pager_threads[use_cpu]) {
		warning("invalid CPU parameter used in pager object");
		return Pager_capability();
	}
	Native_capability pager_thread_cap =
		Capability_space::import(pager_threads[use_cpu]->native_thread().ec_sel);

	/* request creation of portal bind to pager thread */
	Native_capability cap_session =
		_cap_factory.alloc(pager_thread_cap, obj->handler_address(), 0);

	imprint_badge(cap_session.local_name(), reinterpret_cast<mword_t>(obj));

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

	/* cleanup at cap factory */
	_cap_factory.free(pager_obj);

	/* revoke cap selector locally */
	revoke(Capability_space::crd(pager_obj), true);

	/* remove object from pool */
	remove(obj);

	/* take care that no faults are in-flight */
	obj->cleanup_call();
}
