/*
 * \brief  Pager framework
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-25
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rm_session/rm_session.h>

/* base-internal includes */
#include <base/internal/native_thread.h>

/* core includes */
#include <pager.h>
#include <platform.h>
#include <platform_thread.h>
#include <imprint_badge.h>
#include <cpu_thread_component.h>
#include <core_env.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova_util.h> /* map_local */
#include <nova/capability_space.h>

static bool verbose_oom = false;

using namespace Core;
using namespace Nova;


static Nova::Hip const &kernel_hip()
{
	/**
	 * Initial value of esp register, saved by the crt0 startup code.
	 * This value contains the address of the hypervisor information page.
	 */
	extern addr_t __initial_sp;
	return *reinterpret_cast<Hip const *>(__initial_sp);
}


/**
 * Pager threads - one thread per CPU
 */
struct Pager_thread: public Thread
{
	Pager_thread(Affinity::Location location)
	: Thread(Cpu_session::Weight::DEFAULT_WEIGHT, "pager", 2 * 4096, location)
	{
		/* creates local EC */
		Thread::start();

		reinterpret_cast<Nova::Utcb *>(Thread::utcb())->crd_xlt = Obj_crd(0, ~0UL);
	}

	void entry() override { }
};

enum { PAGER_CPUS = Core::Platform::MAX_SUPPORTED_CPUS };
static Constructible<Pager_thread> pager_threads[PAGER_CPUS];

static Pager_thread &pager_thread(Affinity::Location location,
                                  Core::Platform &platform)
{
	unsigned const pager_index = platform.pager_index(location);
	unsigned const kernel_cpu_id = platform.kernel_cpu_id(location);

	if (kernel_hip().is_cpu_enabled(kernel_cpu_id) &&
	    pager_index < PAGER_CPUS && pager_threads[pager_index].constructed())
		return *pager_threads[pager_index];

	warning("invalid CPU parameter used in pager object: ",
	        pager_index, "->", kernel_cpu_id, " location=",
	        location.xpos(), "x", location.ypos(), " ",
	        location.width(), "x", location.height());

	throw Invalid_thread();
}


/**
 * Utility for the formatted output of page-fault information
 */
struct Page_fault_info
{
	char const * const pd;
	char const * const thread;
	unsigned const cpu;
	addr_t const ip, addr, sp;
	uint8_t const pf_type;

	Page_fault_info(char const *pd, char const *thread, unsigned cpu,
	                addr_t ip, addr_t addr, addr_t sp, unsigned type)
	:
		pd(pd), thread(thread), cpu(cpu), ip(ip), addr(addr),
		sp(sp), pf_type((uint8_t)type)
	{ }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "pd='",     pd,      "' "
		                   "thread='", thread,  "' "
		                   "cpu=",     cpu,     " "
		                   "ip=",      Hex(ip), " "
		                   "address=", Hex(addr), " "
		                   "stack pointer=", Hex(sp), " "
		                   "qualifiers=", Hex(pf_type), " ",
		                   pf_type & Ipc_pager::ERR_I ? "I" : "i",
		                   pf_type & Ipc_pager::ERR_R ? "R" : "r",
		                   pf_type & Ipc_pager::ERR_U ? "U" : "u",
		                   pf_type & Ipc_pager::ERR_W ? "W" : "w",
		                   pf_type & Ipc_pager::ERR_P ? "P" : "p");
	}
};


void Pager_object::_page_fault_handler(Pager_object &obj)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	Ipc_pager ipc_pager(utcb, obj.pd_sel(), platform_specific().core_pd_sel());

	/* potential request to ask for EC cap or signal SM cap */
	if (utcb.msg_words() == 1)
		_invoke_handler(obj);

	/*
	 * obj.pager() (pager thread) may issue a signal to the remote region
	 * handler thread which may respond via wake_up() (ep thread) before
	 * we are done here - we have to lock the whole page lookup procedure
	 */
	obj._state_lock.acquire();

	obj._state.thread.ip     = ipc_pager.fault_ip();
	obj._state.thread.sp     = 0;
	obj._state.thread.trapno = PT_SEL_PAGE_FAULT;

	obj._state.block();
	obj._state.block_pause_sm();

	/* lookup fault address and decide what to do */
	unsigned error = (obj.pager(ipc_pager) == Pager_object::Pager_result::STOP);

	/* don't open receive window for pager threads */
	if (utcb.crd_rcv.value())
		nova_die();

	if (!error && ipc_pager.syscall_result() != Nova::NOVA_OK) {
		/* something went wrong - by default don't answer the page fault */
		error = 4;

		/* dst pd has not enough kernel quota ? - try to recover */
		if (ipc_pager.syscall_result() == Nova::NOVA_PD_OOM) {
			uint8_t res = obj.handle_oom();
			if (res == Nova::NOVA_PD_OOM) {
				obj._state.unblock_pause_sm();
				obj._state.unblock();
				obj._state_lock.release();

				/* block until revoke is due */
				ipc_pager.reply_and_wait_for_fault(obj.sel_sm_block_oom());
			} else if (res == Nova::NOVA_OK)
				/* succeeded to recover - continue normally */
				error = 0;
		}
	}

	/* good case - found a valid region which is mappable */
	if (!error) {
		obj._state.unblock_pause_sm();
		obj._state.unblock();
		obj._state_lock.release();
		ipc_pager.reply_and_wait_for_fault();
	}

	char const * const client_thread = obj.client_thread();
	char const * const client_pd     = obj.client_pd();

	unsigned const cpu_id = platform_specific().pager_index(myself.affinity());

	Page_fault_info const fault_info(client_pd, client_thread, cpu_id,
	                                 ipc_pager.fault_ip(),
	                                 ipc_pager.fault_addr(),
	                                 ipc_pager.sp(),
	                                 (uint8_t)ipc_pager.fault_type());
	obj._state_lock.release();

	/* block the faulting thread until region manager is done */
	ipc_pager.reply_and_wait_for_fault(obj.sel_sm_block_pause());
}


void Pager_object::exception(uint8_t exit_id)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	if (exit_id > PT_SEL_PARENT)
		nova_die();

	addr_t const fault_ip = utcb.ip;
	addr_t const fault_sp = utcb.sp;
	addr_t const fault_bp = utcb.bp;

	uint8_t res = 0xFF;
	addr_t  mtd = 0;

	_state_lock.acquire();

	/* remember exception type for Cpu_session::state() calls */
	_state.thread.trapno = exit_id;

	if (_exception_sigh.valid()) {
		_state.submit_signal();
		res = _unsynchronized_client_recall(true);
	}

	if (res != NOVA_OK) {
		/* nobody handles this exception - so thread will be stopped finally */
		_state.mark_dead();

		unsigned const cpu_id = platform_specific().pager_index(myself.affinity());

		warning("unresolvable exception ", exit_id,  ", "
		        "pd '",     client_pd(),            "', "
		        "thread '", client_thread(),        "', "
		        "cpu ",     cpu_id,                  ", "
		        "ip=",      Hex(fault_ip),            " "
		        "sp=",      Hex(fault_sp),            " "
		        "bp=",      Hex(fault_bp),            " ",
		        res == 0xFF ? "no signal handler"
		                    : (res == NOVA_OK ? "" : "recall failed"));

		Nova::revoke(Obj_crd(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2));

		enum { TRAP_BREAKPOINT = 3 };

		if (exit_id == TRAP_BREAKPOINT) {
			utcb.ip = fault_ip - 1;
			mtd     = Mtd::EIP;
		}
	}

	_state_lock.release();

	utcb.set_msg_word(0);
	utcb.mtd = mtd;

	reply(myself.stack_top());
}


bool Pager_object::_migrate_thread()
{
	bool const valid_migrate = (_state.migrate() && _badge);
	if (!valid_migrate)
		return false;

	_state.reset_migrate();

	try {
		/* revoke all exception portals pointing to current pager */
		Platform_thread &thread = *reinterpret_cast<Platform_thread *>(_badge);

		Nova::revoke(Obj_crd(_selectors, 2));

		/* revoke all exception portals selectors */
		Nova::revoke(Obj_crd(exc_pt_sel_client()+0x00, 4));
		Nova::revoke(Obj_crd(exc_pt_sel_client()+0x10, 3));
		Nova::revoke(Obj_crd(exc_pt_sel_client()+0x18, 1));
		Nova::revoke(Obj_crd(exc_pt_sel_client()+0x1f, 0));

		/* re-create portals bound to pager on new target CPU */
		_location   = _next_location;
		_exceptions = Exception_handlers(*this);
		_construct_pager();

		/* map all exception portals to thread pd */
		thread.prepare_migration();

		/* syscall to migrate */
		unsigned const migrate_to = platform_specific().kernel_cpu_id(_location);
		uint8_t res = syscall_retry(*this, [&]() {
			return ec_ctrl(EC_MIGRATE, _state.sel_client_ec, migrate_to,
			               Obj_crd(EC_SEL_THREAD, 0, Obj_crd::RIGHT_EC_RECALL));
		});

		if (res == Nova::NOVA_OK)
			thread.finalize_migration(_location);

		return true;
	} catch (...) {
		return false;
	}
}


void Pager_object::_recall_handler(Pager_object &obj)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	/* acquire mutex */
	obj._state_lock.acquire();

	/* check for migration */
	if (obj._migrate_thread()) {
		/* release mutex */
		obj._state_lock.release();

		utcb.set_msg_word(0);
		utcb.mtd = 0;
		reply(myself.stack_top());
	}

	if (obj._state.modified) {
		obj._copy_state_to_utcb(utcb);
		obj._state.modified = false;
	} else
		utcb.mtd = 0;

	/* switch on/off single step */
	bool singlestep_state = obj._state.thread.eflags & 0x100UL;
	if (obj._state.singlestep() && !singlestep_state) {
		utcb.flags |= 0x100UL;
		utcb.mtd |= Mtd::EFL;
	} else if (!obj._state.singlestep() && singlestep_state) {
		utcb.flags &= ~0x100UL;
		utcb.mtd |= Mtd::EFL;
	}

	/* deliver signal if it was requested */
	if (obj._state.to_submit())
		obj.submit_exception_signal();

	/* block until Cpu_session()::resume() respectively wake_up() call */

	unsigned long sm = 0;

	if (obj._state.blocked()) {
		sm = obj.sel_sm_block_pause();
		obj._state.block_pause_sm();
	}

	obj._state_lock.release();

	utcb.set_msg_word(0);
	reply(myself.stack_top(), sm);
}


void Pager_object::_startup_handler(Pager_object &obj)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	utcb.ip  = obj._initial_eip;
	utcb.sp  = obj._initial_esp;
	utcb.mtd = Mtd::EIP | Mtd::ESP;

	if (obj._state.singlestep()) {
		utcb.flags = 0x100UL;
		utcb.mtd |= Mtd::EFL;
	}

	obj._state.unblock();

	utcb.set_msg_word(0);

	reply(myself.stack_top());
}


void Pager_object::_invoke_handler(Pager_object &obj)
{
	Thread &myself = *Thread::myself();
	Utcb   &utcb   = *reinterpret_cast<Utcb *>(myself.utcb());

	/* receive window must be closed - otherwise implementation bug */
	if (utcb.crd_rcv.value())
		nova_die();

	/* if protocol is violated ignore request */
	if (utcb.msg_words() != 1) {
		utcb.mtd = 0;
		utcb.set_msg_word(0);
		reply(myself.stack_top());
	}

	addr_t const event = utcb.msg()[0];

	/* check for translated pager portals - required for vCPU in remote PDs */
	if (utcb.msg_items() == 1 && utcb.msg_words() == 1 && event == 0xaffe) {

		Nova::Utcb::Item const &item = *utcb.get_item(0);
		Nova::Crd const cap(item.crd);

		/* valid item which got translated ? */
		if (!cap.is_null() && !item.is_del()) {
			Rpc_entrypoint &e = core_env().entrypoint();
			e.apply(cap.base(),
				[&] (Cpu_thread_component *source) {
					if (!source)
						return;

					Platform_thread &p = source->platform_thread();
					addr_t const sel_exc_base = p.remote_vcpu();
					if (sel_exc_base == Native_thread::INVALID_INDEX)
						return;

					/* delegate VM-exit portals */
					map_vcpu_portals(p.pager(), sel_exc_base, sel_exc_base,
					                 utcb, obj.pd_sel());

					/* delegate portal to contact pager */
					map_pagefault_portal(obj, p.pager().exc_pt_sel_client(),
					                     sel_exc_base, obj.pd_sel(), utcb);
				});
		}

		utcb.mtd = 0;
		utcb.set_msg_word(0);
		reply(myself.stack_top());
	}

	utcb.mtd = 0;
	utcb.set_msg_word(0);

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
		bool res = utcb.append_item(Obj_crd(obj._state.sel_client_ec, 0,
		                                    Obj_crd::RIGHT_EC_RECALL), 0);
		(void)res;
	}

	/* semaphore for signaling thread is requested, reuse PT_SEL_STARTUP. */
	if (event == ~0UL - 1) {
		/* create semaphore only once */
		if (!obj._state.has_signal_sm()) {

			revoke(Obj_crd(obj.exc_pt_sel_client() + PT_SEL_STARTUP, 0));

			bool res = Nova::create_sm(obj.exc_pt_sel_client() + PT_SEL_STARTUP,
			                           platform_specific().core_pd_sel(), 0);
			if (res != Nova::NOVA_OK)
				reply(myself.stack_top());

			obj._state.mark_signal_sm();
		}

		bool res = utcb.append_item(Obj_crd(obj.exc_pt_sel_client() +
		                                    PT_SEL_STARTUP, 0), 0);
		(void)res;
	}

	reply(myself.stack_top());
}


void Pager_object::wake_up()
{
	Mutex::Guard _state_lock_guard(_state_lock);

	if (!_state.blocked())
		return;

	_state.thread.exception = false;

	_state.unblock();

	if (_state.blocked_pause_sm()) {

		uint8_t res = sm_ctrl(sel_sm_block_pause(), SEMAPHORE_UP);

		if (res == NOVA_OK)
			_state.unblock_pause_sm();
		else
			warning("canceling blocked client failed (thread sm)");
	}
}


uint8_t Pager_object::client_recall(bool get_state_and_block)
{
	Mutex::Guard _state_lock_guard(_state_lock);
	return _unsynchronized_client_recall(get_state_and_block);
}


uint8_t Pager_object::_unsynchronized_client_recall(bool get_state_and_block)
{
	enum { STATE_REQUESTED = 1UL, STATE_INVALID = ~0UL };

	uint8_t res = ec_ctrl(EC_RECALL, _state.sel_client_ec,
	                      get_state_and_block ? STATE_REQUESTED : STATE_INVALID);

	if (res != NOVA_OK)
		return res;

	if (get_state_and_block) {
		Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
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

	Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	utcb.set_msg_word(0);
	utcb.mtd = 0;
	if (uint8_t res = call(sel_pt_cleanup()))
		error(&utcb, " - cleanup call to pager failed res=", res);
}


void Pager_object::print(Output &out) const
{
	Platform_thread const * const faulter = reinterpret_cast<Platform_thread *>(_badge);
	Genode::print(out, "pager_object: pd='",
			faulter ? faulter->pd_name() : "unknown", "' thread='",
			faulter ? faulter->name() : "unknown", "'");
}


static uint8_t create_portal(addr_t pt, addr_t pd, addr_t ec, Mtd mtd,
                             addr_t eip, Pager_object * oom_handler)
{
	uint8_t res = syscall_retry(*oom_handler,
		[&]() { return create_pt(pt, pd, ec, mtd, eip); });

	if (res != NOVA_OK)
		return res;

	addr_t const badge_localname = reinterpret_cast<addr_t>(oom_handler);

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
void Exception_handlers::register_handler(Pager_object &obj, Mtd mtd,
                                          void (* __attribute__((regparm(1))) func)(Pager_object &))
{
	addr_t const ec_sel = pager_thread(obj.location(), platform_specific()).native_thread().ec_sel;

	/* compiler generates instance of exception entry if not specified */
	addr_t entry = func ? (addr_t)func : (addr_t)(&_handler<EV>);
	uint8_t res = create_portal(obj.exc_pt_sel_client() + EV,
	                            platform_specific().core_pd_sel(), ec_sel, mtd, entry, &obj);
	if (res != Nova::NOVA_OK)
		throw Invalid_thread();
}


template <uint8_t EV>
void Exception_handlers::_handler(Pager_object &obj)
{
	obj.exception(EV);
}


Exception_handlers::Exception_handlers(Pager_object &obj)
{
	Mtd const mtd (Mtd::EBSD | Mtd::ESP | Mtd::EIP);

	register_handler<0>(obj, mtd);
	register_handler<1>(obj, mtd);
	register_handler<2>(obj, mtd);
	register_handler<3>(obj, mtd);
	register_handler<4>(obj, mtd);
	register_handler<5>(obj, mtd);
	register_handler<6>(obj, mtd);
	register_handler<7>(obj, mtd);
	register_handler<8>(obj, mtd);
	register_handler<9>(obj, mtd);
	register_handler<10>(obj, mtd);
	register_handler<11>(obj, mtd);
	register_handler<12>(obj, mtd);
	register_handler<13>(obj, mtd);

	register_handler<15>(obj, mtd);
	register_handler<16>(obj, mtd);
	register_handler<17>(obj, mtd);
	register_handler<18>(obj, mtd);
	register_handler<19>(obj, mtd);
	register_handler<20>(obj, mtd);
	register_handler<21>(obj, mtd);
	register_handler<22>(obj, mtd);
	register_handler<23>(obj, mtd);
	register_handler<24>(obj, mtd);
	register_handler<25>(obj, mtd);
}


/******************
 ** Pager object **
 ******************/

void Pager_object::_construct_pager()
{
	addr_t const pd_sel = platform_specific().core_pd_sel();
	addr_t const ec_sel = pager_thread(_location, platform_specific()).native_thread().ec_sel;

	/* create portal for page-fault handler - 14 */
	_exceptions.register_handler<14>(*this, Mtd::QUAL | Mtd::ESP | Mtd::EIP,
	                                 _page_fault_handler);

	/* create portal for recall handler */
	Mtd const mtd_recall(Mtd::ESP | Mtd::EIP | Mtd::ACDB | Mtd::EFL |
	                     Mtd::EBSD | Mtd::FSGS);
	_exceptions.register_handler<PT_SEL_RECALL>(*this, mtd_recall,
	                                            _recall_handler);

	/* create portal for final cleanup call used during destruction */
	uint8_t res = create_portal(sel_pt_cleanup(), pd_sel, ec_sel, Mtd(0),
	                            reinterpret_cast<addr_t>(_invoke_handler),
	                            this);
	if (res != Nova::NOVA_OK) {
		error("could not create pager cleanup portal, error=", res);
		throw Invalid_thread();
	}

	/* semaphore used to block paged thread during recall */
	res = Nova::create_sm(sel_sm_block_pause(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Invalid_thread();
	}

	/* semaphore used to block paged thread during OOM memory revoke */
	res = Nova::create_sm(sel_sm_block_oom(), pd_sel, 0);
	if (res != Nova::NOVA_OK) {
		throw Invalid_thread();
	}
}


Pager_object::Pager_object(Cpu_session_capability cpu_session_cap,
                           Thread_capability thread_cap, unsigned long badge,
                           Affinity::Location location, Session_label const &,
                           Cpu_session::Name const &)
:
	_badge(badge),
	_selectors(cap_map().insert(2)),
	_client_exc_pt_sel(cap_map().insert(NUM_INITIAL_PT_LOG2)),
	_cpu_session_cap(cpu_session_cap), _thread_cap(thread_cap),
	_location(location),
	_exceptions(*this),
	_pd_target(Native_thread::INVALID_INDEX)
{
	_state._status       = 0;
	_state.modified      = false;
	_state.sel_client_ec = Native_thread::INVALID_INDEX;
	_state.block();

	if (Native_thread::INVALID_INDEX == _selectors ||
	    Native_thread::INVALID_INDEX == _client_exc_pt_sel)
		throw Invalid_thread();

	_construct_pager();

	/* create portal for startup handler */
	Mtd const mtd_startup(Mtd::ESP | Mtd::EIP);
	_exceptions.register_handler<PT_SEL_STARTUP>(*this, mtd_startup,
	                                             _startup_handler);

	/*
	 * Create semaphore required for Genode locking. It can be later on
	 * requested by the thread the same way as all exception portals.
	 */
	addr_t const pd_sel = platform_specific().core_pd_sel();
	uint8_t const res = Nova::create_sm(exc_pt_sel_client() + SM_SEL_EC,
	                                    pd_sel, 0);
	if (res != Nova::NOVA_OK)
		throw Invalid_thread();
}


void Pager_object::migrate(Affinity::Location location)
{
	Mutex::Guard _state_lock_guard(_state_lock);

	if (_state.blocked())
		return;

	if (location.xpos() == _location.xpos() &&
	    location.ypos() == _location.ypos())
		return;

	/* initiate migration by recall handler */
	bool const just_recall = false;
	uint8_t const res = _unsynchronized_client_recall(just_recall);
	if (res == Nova::NOVA_OK) {
		_next_location = location;

		_state.request_migrate();
	}
}


Pager_object::~Pager_object()
{
	/* sanity check that object got dissolved already - otherwise bug */
	if (!_state.dissolved())
		nova_die();

	/* revoke portal used for the cleanup call and sm cap for blocking state */
	revoke(Obj_crd(_selectors, 2));
	cap_map().remove(_selectors, 2, false);
	cap_map().remove(exc_pt_sel_client(), NUM_INITIAL_PT_LOG2, false);
}


uint8_t Pager_object::handle_oom(addr_t transfer_from,
                                 char const * src_pd, char const * src_thread,
                                 enum Pager_object::Policy policy)
{
	return handle_oom(transfer_from, pd_sel(), src_pd, src_thread, policy,
	                  sel_sm_block_oom(), client_pd(), client_thread());
}

uint8_t Pager_object::handle_oom(addr_t pd_from, addr_t pd_to,
                                 char const * src_pd, char const * src_thread,
                                 Policy policy, addr_t sm_notify,
                                 char const * dst_pd, char const * dst_thread)
{
	addr_t const core_pd_sel = platform_specific().core_pd_sel();

	enum { QUOTA_TRANSFER_PAGES = 2 };

	if (pd_from == SRC_CORE_PD)
		pd_from = core_pd_sel;

	/* request current kernel quota usage of target pd */
	addr_t limit_before = 0, usage_before = 0;
	Nova::pd_ctrl_debug(pd_to, limit_before, usage_before);

	if (verbose_oom) {
		addr_t limit_source = 0, usage_source = 0;
		/* request current kernel quota usage of source pd */
		Nova::pd_ctrl_debug(pd_from, limit_source, usage_source);

		log("oom - '", dst_pd, "':'", dst_thread, "' "
		    "(", usage_before, "/", limit_before, ") - "
		    "transfer ", (long)QUOTA_TRANSFER_PAGES, " pages "
		    "from '", src_pd, "':'", src_thread, "' "
		    "(", usage_source, "/", limit_source, ")");
	}

	uint8_t res = Nova::NOVA_PD_OOM;

	if (pd_from != pd_to) {
		/* upgrade quota */
		uint8_t res = Nova::pd_ctrl(pd_from, Pd_op::TRANSFER_QUOTA,
		                            pd_to, QUOTA_TRANSFER_PAGES);
		if (res == Nova::NOVA_OK)
			return res;
	}

	/* retry upgrade using core quota if policy permits */
	if (policy == UPGRADE_PREFER_SRC_TO_DST) {
		if (pd_from != core_pd_sel) {
			res = Nova::pd_ctrl(core_pd_sel, Pd_op::TRANSFER_QUOTA,
			                    pd_to, QUOTA_TRANSFER_PAGES);
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
	Nova::revoke(crd_all, PD_SELF, REMOTE_REVOKE, pd_to, sm_notify);

	/* re-request current kernel quota usage of target pd */
	addr_t limit_after = 0, usage_after = 0;
	Nova::pd_ctrl_debug(pd_to, limit_after, usage_after);
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

	Thread       &myself  = *Thread::myself();
	Utcb         &utcb    = *reinterpret_cast<Utcb *>(myself.utcb());
	Pager_object &obj_dst = *reinterpret_cast<Pager_object *>(pager_dst);
	Pager_object &obj_src = *reinterpret_cast<Pager_object *>(pager_src);

	/* Policy used if the Process of the paged thread runs out of memory */
	enum Policy policy = Policy::UPGRADE_CORE_TO_DST;


	/* check assertions - cases that should not happen on Genode@Nova */
	enum { NO_OOM_PT = 0UL };

	/* all relevant (user) threads should have a OOM PT */
	bool assert = pager_dst == NO_OOM_PT;

	/*
	 * This pager thread does solely reply to IPC calls - it should never
	 * cause OOM during the sending phase of a IPC.
	 */
	assert |= ((reason & (SELF | SEND)) == (SELF | SEND));

	/*
	 * This pager thread should never send words (untyped items) - it just
	 * answers page faults by typed items (memory mappings).
	 */
	assert |= utcb.msg_words();

	if (assert) {
		error("unknown OOM case - stop core pager thread");
		utcb.set_msg_word(0);
		reply(myself.stack_top(), myself.native_thread().exc_pt_sel + Nova::SM_SEL_EC);
	}

	/* be strict in case of the -strict- STOP policy - stop causing thread */
	if (policy == STOP) {
		error("PD has insufficient kernel memory left - stop thread");
		utcb.set_msg_word(0);
		reply(myself.stack_top(), obj_dst.sel_sm_block_pause());
	}

	char const * src_pd     = "core";
	char const * src_thread = "pager";

	addr_t transfer_from = SRC_CORE_PD;

	switch (pager_src) {
	case SRC_PD_UNKNOWN:
		/* should not happen on Genode - we create and know every PD in core */
		error("Unknown PD has insufficient kernel memory left - stop thread");
		utcb.set_msg_word(0);
		reply(myself.stack_top(), myself.native_thread().exc_pt_sel + Nova::SM_SEL_EC);

	case SRC_CORE_PD:
		/* core PD -> other PD, which has insufficient kernel resources */

		if (!(reason & SELF)) {
			/* case that src thread != this thread in core */
			src_thread = "unknown";
			utcb.set_msg_word(0);
        }

		transfer_from = platform_specific().core_pd_sel();
		break;
	default:
		/* non core PD -> non core PD */
		utcb.set_msg_word(0);

		if (pager_src == pager_dst || policy == UPGRADE_CORE_TO_DST)
			transfer_from = platform_specific().core_pd_sel();
		else {
			/* delegation of items between different PDs */
			src_pd        = obj_src.client_pd();
			src_thread    = obj_src.client_thread();
			transfer_from = obj_src.pd_sel();
		}
	}

	uint8_t res = obj_dst.handle_oom(transfer_from, src_pd, src_thread, policy);
	if (res == Nova::NOVA_OK)
		/* handling succeeded - continue with original IPC */
		reply(myself.stack_top());

	/* transfer nothing */
	utcb.set_msg_word(0);

	if (res != Nova::NOVA_PD_OOM)
		error("upgrading kernel memory failed, policy ", (int)policy, ", "
		      "error ", (int)res, " - stop thread finally");

	/* else: caller will get blocked until RCU period is over */

	/* block caller in semaphore */
	reply(myself.stack_top(), obj_dst.sel_sm_block_oom());
}


addr_t Pager_object::create_oom_portal()
{
	try {
		addr_t const pt_oom      = sel_oom_portal();
		addr_t const core_pd_sel = platform_specific().core_pd_sel();
		Pager_thread &thread     = pager_thread(_location, platform_specific());
		addr_t const ec_sel      = thread.native_thread().ec_sel;

		uint8_t res = create_portal(pt_oom, core_pd_sel, ec_sel, Mtd(0),
		                            reinterpret_cast<addr_t>(_oom_handler),
		                            this);
		if (res == Nova::NOVA_OK)
			return pt_oom;
	} catch (...) { }

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
 ** Pager entrypoint **
 **********************/

Pager_entrypoint::Pager_entrypoint(Rpc_cap_factory &)
{
	/* sanity check for pager threads */
	if (kernel_hip().cpu_max() > PAGER_CPUS) {
		error("kernel supports more CPUs (", kernel_hip().cpu_max(), ") "
		      "than Genode (", (unsigned)PAGER_CPUS, ")");
		nova_die();
	}

	/* detect enabled CPUs and create per CPU a pager thread */
	platform_specific().for_each_location([&](Affinity::Location &location) {
		unsigned const pager_index = platform_specific().pager_index(location);
		unsigned const kernel_cpu_id = platform_specific().kernel_cpu_id(location);

		if (!kernel_hip().is_cpu_enabled(kernel_cpu_id))
			return;

		pager_threads[pager_index].construct(location);
	});
}


void Pager_entrypoint::dissolve(Pager_object &obj)
{
	/* take care that no faults are in-flight */
	obj.cleanup_call();
}
