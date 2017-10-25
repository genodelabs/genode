/*
 * \brief  Kernel back-end for execution contexts in userland
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread_state.h>
#include <cpu_session/cpu_session.h>
#include <util/construct_at.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>
#include <base/internal/native_utcb.h>
#include <base/internal/crt0.h>

/* core includes */
#include <hw/assert.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <kernel/log.h>
#include <map_local.h>
#include <platform_pd.h>
#include <pic.h>

extern "C" void _core_start(void);

using namespace Kernel;


void Thread_fault::print(Genode::Output &out) const
{
	Genode::print(out, "ip=",          Genode::Hex(ip));
	Genode::print(out, " fault-addr=", Genode::Hex(addr));
	Genode::print(out, " type=");
	switch (type) {
		case WRITE:        Genode::print(out, "write-fault"); return;
		case EXEC:         Genode::print(out, "exec-fault"); return;
		case PAGE_MISSING: Genode::print(out, "no-page"); return;
		case UNKNOWN:      Genode::print(out, "unknown"); return;
	};
}


void Thread::_signal_context_kill_pending()
{
	assert(_state == ACTIVE);
	_become_inactive(AWAITS_SIGNAL_CONTEXT_KILL);
}


void Thread::_signal_context_kill_done()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	user_arg_0(0);
	_become_active();
}


void Thread::_signal_context_kill_failed()
{
	assert(_state == AWAITS_SIGNAL_CONTEXT_KILL);
	user_arg_0(-1);
	_become_active();
}


void Thread::_await_signal(Signal_receiver * const receiver)
{
	_become_inactive(AWAITS_SIGNAL);
	_signal_receiver = receiver;
}


void Thread::_receive_signal(void * const base, size_t const size)
{
	assert(_state == AWAITS_SIGNAL);
	Genode::memcpy(utcb()->data(), base, size);
	_become_active();
}


void Thread::_send_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(0);
	_state = ACTIVE;
	if (!Cpu_job::own_share_active()) { _activate_used_shares(); }
}


void Thread::_send_request_failed()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(-1);
	_state = ACTIVE;
	if (!Cpu_job::own_share_active()) { _activate_used_shares(); }
}


void Thread::_await_request_succeeded()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(0);
	_become_active();
}


void Thread::_await_request_failed()
{
	assert(_state == AWAITS_IPC);
	user_arg_0(-1);
	_become_active();
}


void Thread::_deactivate_used_shares()
{
	Cpu_job::_deactivate_own_share();
	Ipc_node::for_each_helper([&] (Ipc_node * const h) {
		static_cast<Thread *>(h)->_deactivate_used_shares(); });
}

void Thread::_activate_used_shares()
{
	Cpu_job::_activate_own_share();
	Ipc_node::for_each_helper([&] (Ipc_node * const h) {
		static_cast<Thread *>(h)->_activate_used_shares(); });
}

void Thread::_become_active()
{
	if (_state != ACTIVE && !_paused) { _activate_used_shares(); }
	_state = ACTIVE;
}


void Thread::_become_inactive(State const s)
{
	if (_state == ACTIVE && !_paused) { _deactivate_used_shares(); }
	_state = s;
}


void Thread::_die() { _become_inactive(DEAD); }


Cpu_job * Thread::helping_sink() {
	return static_cast<Thread *>(Ipc_node::helping_sink()); }


size_t Thread::_core_to_kernel_quota(size_t const quota) const
{
	using Genode::Cpu_session;
	using Genode::sizet_arithm_t;
	size_t const ticks = _cpu->us_to_ticks(Kernel::cpu_quota_us);
	return Cpu_session::quota_lim_downscale<sizet_arithm_t>(quota, ticks);
}


void Thread::_call_new_thread()
{
	void *       const p        = (void *)user_arg_1();
	unsigned     const priority = user_arg_2();
	unsigned     const quota    = _core_to_kernel_quota(user_arg_3());
	char const * const label    = (char *)user_arg_4();
	Core_object<Thread> * co =
		Genode::construct_at<Core_object<Thread> >(p, priority, quota, label);
	user_arg_0(co->core_capid());
}


void Thread::_call_new_core_thread()
{
	void *       const p        = (void *)user_arg_1();
	char const * const label    = (char *)user_arg_2();
	Core_object<Thread> * co =
		Genode::construct_at<Core_object<Thread> >(p, label);
	user_arg_0(co->core_capid());
}


void Thread::_call_thread_quota()
{
	Thread * const thread = (Thread *)user_arg_1();
	thread->Cpu_job::quota(_core_to_kernel_quota(user_arg_2()));
}


void Thread::_call_start_thread()
{
	/* lookup CPU */
	Cpu * const cpu = cpu_pool()->cpu(user_arg_2());
	if (!cpu) {
		Genode::warning("failed to lookup CPU");
		user_arg_0(-2);
		return;
	}
	user_arg_0(0);
	Thread * const thread = (Thread*) user_arg_1();

	assert(thread->_state == AWAITS_START)

	thread->affinity(cpu);

	/* join protection domain */
	thread->_pd = (Pd *) user_arg_3();
	thread->Ipc_node::_init((Native_utcb *)user_arg_4(), this);
	thread->_become_active();
}


void Thread::_call_pause_thread()
{
	Thread &thread = *reinterpret_cast<Thread*>(user_arg_1());
	if (thread._state == ACTIVE && !thread._paused) {
		thread._deactivate_used_shares(); }

	thread._paused = true;
}


void Thread::_call_resume_thread()
{
	Thread &thread = *reinterpret_cast<Thread*>(user_arg_1());
	if (thread._state == ACTIVE && thread._paused) {
		thread._activate_used_shares(); }

	thread._paused = false;
}


void Thread::_call_stop_thread()
{
	assert(_state == ACTIVE);
	_become_inactive(AWAITS_RESTART);
}


void Thread::_call_restart_thread()
{
	if (!pd()) {
		return; }

	Thread * const thread = pd()->cap_tree().find<Thread>(user_arg_1());
	if (!thread || (!_core && (pd() != thread->pd()))) {
		warning(*this, ": failed to lookup thread ", (unsigned)user_arg_1(),
		        " to restart it");
		_die();
		return;
	}
	user_arg_0(thread->_restart());
}


bool Thread::_restart()
{
	assert(_state == ACTIVE || _state == AWAITS_RESTART);
	if (_state != AWAITS_RESTART) { return false; }
	_become_active();
	return true;
}


void Thread::_call_cancel_thread_blocking()
{
	reinterpret_cast<Thread*>(user_arg_1())->_cancel_blocking();
}


void Thread::_cancel_blocking()
{
	switch (_state) {
	case AWAITS_RESTART:
		_become_active();
		return;
	case AWAITS_IPC:
		Ipc_node::cancel_waiting();
		return;
	case AWAITS_SIGNAL:
		Signal_handler::cancel_waiting();
		user_arg_0(-1);
		_become_active();
		return;
	case AWAITS_SIGNAL_CONTEXT_KILL:
		Signal_context_killer::cancel_waiting();
		return;
	case ACTIVE:
		return;
	case DEAD:
		Genode::error("can't cancel blocking of dead thread");
		return;
	case AWAITS_START:
		Genode::error("can't cancel blocking of not yet started thread");
		return;
	}
}


void Thread::_call_yield_thread()
{
	Cpu_job::_yield();
}


void Thread::_call_await_request_msg()
{
	if (Ipc_node::await_request(user_arg_1())) {
		user_arg_0(0);
		return;
	}
	_become_inactive(AWAITS_IPC);
}


void Thread::_call_timeout()
{
	_timeout_sigid = user_arg_2();
	Cpu_job::timeout(this, user_arg_1());
}


void Thread::_call_timeout_age_us()
{
	user_arg_0(Cpu_job::timeout_age_us(this));
}

void Thread::_call_timeout_max_us()
{
	user_arg_0(Cpu_job::timeout_max_us());
}


void Thread::timeout_triggered()
{
	Signal_context * const c =
		pd()->cap_tree().find<Signal_context>(_timeout_sigid);
	if (!c || c->submit(1))
		Genode::warning(*this, ": failed to submit timeout signal");
}


void Thread::_call_send_request_msg()
{
	Object_identity_reference * oir = pd()->cap_tree().find(user_arg_1());
	Thread * const dst = (oir) ? oir->object<Thread>() : nullptr;
	if (!dst) {
		Genode::warning(*this, ": cannot send to unknown recipient ",
		                 (unsigned)user_arg_1());
		_become_inactive(AWAITS_IPC);
		return;
	}
	bool const help = Cpu_job::_helping_possible(dst);
	oir = oir->find(dst->pd());

	Ipc_node::send_request(dst, oir ? oir->capid() : cap_id_invalid(),
	                       help, user_arg_2());
	_state = AWAITS_IPC;
	if (!help || !dst->own_share_active()) { _deactivate_used_shares(); }
}


void Thread::_call_send_reply_msg()
{
	Ipc_node::send_reply();
	bool const await_request_msg = user_arg_2();
	if (await_request_msg) { _call_await_request_msg(); }
	else { user_arg_0(0); }
}


void Thread::_call_pager()
{
	/* override event route */
	Thread * const t = (Thread*) user_arg_1();
	t->_pager = pd()->cap_tree().find<Signal_context>(user_arg_2());
}


void Thread::_call_print_char() { Kernel::log((char)user_arg_1()); }


void Thread::_call_await_signal()
{
	/* cancel if another thread set our "cancel next await signal" bit */
	if (_cancel_next_await_signal) {
		user_arg_0(-1);
		_cancel_next_await_signal = false;
		return;
	}
	/* lookup receiver */
	Signal_receiver * const r = pd()->cap_tree().find<Signal_receiver>(user_arg_1());
	if (!r) {
		Genode::warning(*this, ": cannot await, unknown signal receiver ",
		                (unsigned)user_arg_1());
		user_arg_0(-1);
		return;
	}
	/* register handler at the receiver */
	if (r->add_handler(this)) {
		Genode::warning("failed to register handler at signal receiver");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


void Thread::_call_cancel_next_await_signal()
{
	/* kill the caller if he has no protection domain */
	if (!pd()) {
		error(*this, ": PD not set");
		_die();
		return;
	}
	/* kill the caller if the capability of the target thread is invalid */
	Thread * const thread = pd()->cap_tree().find<Thread>(user_arg_1());
	if (!thread || pd() != thread->pd()) {
		error(*this, ": failed to lookup thread ", (unsigned)user_arg_1());
		_die();
		return;
	}
	/* resume the target thread directly if it blocks for signals */
	if (thread->_state == AWAITS_SIGNAL) {
		thread->_cancel_blocking();
		return;
	}
	/* if not, keep in mind to cancel its next signal blocking */
	thread->_cancel_next_await_signal = true;
}


void Thread::_call_submit_signal()
{
	/* lookup signal context */
	Signal_context * const c = pd()->cap_tree().find<Signal_context>(user_arg_1());
	if(!c) {
		Genode::warning(*this, ": cannot submit unknown signal context");
		user_arg_0(-1);
		return;
	}

	/* trigger signal context */
	if (c->submit(user_arg_2())) {
		Genode::warning("failed to submit signal context");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


void Thread::_call_ack_signal()
{
	/* lookup signal context */
	Signal_context * const c = pd()->cap_tree().find<Signal_context>(user_arg_1());
	if (!c) {
		Genode::warning(*this, ": cannot ack unknown signal context");
		return;
	}

	/* acknowledge */
	c->ack();
}


void Thread::_call_kill_signal_context()
{
	/* lookup signal context */
	Signal_context * const c = pd()->cap_tree().find<Signal_context>(user_arg_1());
	if (!c) {
		Genode::warning(*this, ": cannot kill unknown signal context");
		user_arg_0(-1);
		return;
	}

	/* kill signal context */
	if (c->kill(this)) {
		Genode::warning("failed to kill signal context");
		user_arg_0(-1);
		return;
	}
}


void Thread::_call_new_irq()
{
	Signal_context * const c = pd()->cap_tree().find<Signal_context>(user_arg_3());
	if (!c) {
		Genode::warning(*this, ": invalid signal context for interrupt");
		user_arg_0(-1);
		return;
	}

	new ((void *)user_arg_1()) User_irq(user_arg_2(), *c);
	user_arg_0(0);
}


void Thread::_call_ack_irq() {
	reinterpret_cast<User_irq*>(user_arg_1())->enable(); }


void Thread::_call_new_obj()
{
	/* lookup thread */
	Object_identity_reference * ref = pd()->cap_tree().find(user_arg_2());
	Thread * thread = ref ? ref->object<Thread>() : nullptr;
	if (!thread ||
		(static_cast<Core_object<Thread>*>(thread)->capid() != ref->capid())) {
		if (thread)
			Genode::warning("faked thread", thread);
		user_arg_0(cap_id_invalid());
		return;
	}

	using Thread_identity = Core_object_identity<Thread>;
	Thread_identity * coi =
		Genode::construct_at<Thread_identity>((void *)user_arg_1(), *thread);
	user_arg_0(coi->core_capid());
}


void Thread::_call_delete_obj()
{
	using Object = Core_object_identity<Thread>;
	reinterpret_cast<Object*>(user_arg_1())->~Object();
}


void Thread::_call_ack_cap()
{
	Object_identity_reference * oir = pd()->cap_tree().find(user_arg_1());
	if (oir) oir->remove_from_utcb();
}


void Thread::_call_delete_cap()
{
	Object_identity_reference * oir = pd()->cap_tree().find(user_arg_1());
	if (!oir) return;

	if (oir->in_utcb()) return;

	destroy(pd()->platform_pd()->capability_slab(), oir);
}


void Thread::_call()
{
	try {

	/* switch over unrestricted kernel calls */
	unsigned const call_id = user_arg_0();
	switch (call_id) {
	case call_id_update_data_region():       _call_update_data_region(); return;
	case call_id_update_instr_region():      _call_update_instr_region(); return;
	case call_id_stop_thread():              _call_stop_thread(); return;
	case call_id_restart_thread():           _call_restart_thread(); return;
	case call_id_yield_thread():             _call_yield_thread(); return;
	case call_id_send_request_msg():         _call_send_request_msg(); return;
	case call_id_send_reply_msg():           _call_send_reply_msg(); return;
	case call_id_await_request_msg():        _call_await_request_msg(); return;
	case call_id_kill_signal_context():      _call_kill_signal_context(); return;
	case call_id_submit_signal():            _call_submit_signal(); return;
	case call_id_await_signal():             _call_await_signal(); return;
	case call_id_cancel_next_await_signal(): _call_cancel_next_await_signal(); return;
	case call_id_ack_signal():               _call_ack_signal(); return;
	case call_id_print_char():               _call_print_char(); return;
	case call_id_ack_cap():                  _call_ack_cap(); return;
	case call_id_delete_cap():               _call_delete_cap(); return;
	case call_id_timeout():                  _call_timeout(); return;
	case call_id_timeout_age_us():           _call_timeout_age_us(); return;
	case call_id_timeout_max_us():           _call_timeout_max_us(); return;
	case call_id_time():                     user_arg_0(Cpu_job::time()); return;
	default:
		/* check wether this is a core thread */
		if (!_core) {
			Genode::warning(*this, ": not entitled to do kernel call");
			_die();
			return;
		}
	}
	/* switch over kernel calls that are restricted to core */
	switch (call_id) {
	case call_id_new_thread():             _call_new_thread(); return;
	case call_id_new_core_thread():        _call_new_core_thread(); return;
	case call_id_thread_quota():           _call_thread_quota(); return;
	case call_id_delete_thread():          _call_delete<Thread>(); return;
	case call_id_start_thread():           _call_start_thread(); return;
	case call_id_resume_thread():          _call_resume_thread(); return;
	case call_id_cancel_thread_blocking(): _call_cancel_thread_blocking(); return;
	case call_id_thread_pager():           _call_pager(); return;
	case call_id_update_pd():              _call_update_pd(); return;
	case call_id_new_pd():
		_call_new<Pd>((Hw::Page_table *)      user_arg_2(),
		              (Genode::Platform_pd *) user_arg_3());
		return;
	case call_id_delete_pd():              _call_delete<Pd>(); return;
	case call_id_new_signal_receiver():    _call_new<Signal_receiver>(); return;
	case call_id_new_signal_context():
		_call_new<Signal_context>((Signal_receiver*) user_arg_2(), user_arg_3());
		return;
	case call_id_delete_signal_context():  _call_delete<Signal_context>(); return;
	case call_id_delete_signal_receiver(): _call_delete<Signal_receiver>(); return;
	case call_id_new_vm():                 _call_new_vm(); return;
	case call_id_delete_vm():              _call_delete_vm(); return;
	case call_id_run_vm():                 _call_run_vm(); return;
	case call_id_pause_vm():               _call_pause_vm(); return;
	case call_id_pause_thread():           _call_pause_thread(); return;
	case call_id_new_irq():                _call_new_irq(); return;
	case call_id_delete_irq():             _call_delete<Irq>(); return;
	case call_id_ack_irq():                _call_ack_irq(); return;
	case call_id_new_obj():                _call_new_obj(); return;
	case call_id_delete_obj():             _call_delete_obj(); return;
	default:
		Genode::warning(*this, ": unknown kernel call");
		_die();
		return;
	}
	} catch (Genode::Allocator::Out_of_memory &e) { user_arg_0(-2); }
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESTART);
	Cpu::mmu_fault(*regs, _fault);
	_fault.ip = regs->ip;

	if (_fault.type == Thread_fault::UNKNOWN) {
		Genode::error(*this, " raised unhandled MMU fault ", _fault);
		return;
	}

	if (_core)
		Genode::error(*this, " raised a fault, which should never happen ",
	                  _fault);

	if (_pager) _pager->submit(1);
}


Thread::Thread(unsigned const priority, unsigned const quota,
               char const * const label, bool core)
:
	Cpu_job(priority, quota), _state(AWAITS_START),
	_signal_receiver(0), _label(label), _core(core), regs(core) { }


void Thread::print(Genode::Output &out) const
{
	Genode::print(out, (_pd) ? _pd->platform_pd()->label() : "?");
	Genode::print(out, " -> ");
	Genode::print(out, label());
}


Genode::uint8_t __initial_stack_base[DEFAULT_STACK_SIZE];


/*****************
 ** Core_thread **
 *****************/

Core_thread::Core_thread()
: Core_object<Thread>("core")
{
	using namespace Genode;

	static Native_utcb * const utcb =
		unmanaged_singleton<Native_utcb, get_page_size()>();
	static addr_t const utcb_phys = Platform::core_phys_addr((addr_t)utcb);

	/* map UTCB */
	Genode::map_local(utcb_phys, (addr_t)utcb_main_thread(),
	                  sizeof(Native_utcb) / get_page_size());

	utcb->cap_add(core_capid());
	utcb->cap_add(cap_id_invalid());
	utcb->cap_add(cap_id_invalid());

	/* start thread with stack pointer at the top of stack */
	regs->sp = (addr_t)&__initial_stack_base[0] + DEFAULT_STACK_SIZE;
	regs->ip = (addr_t)&_core_start;

	affinity(cpu_pool()->primary_cpu());
	_utcb       = utcb;
	Thread::_pd = core_pd();
	_become_active();
}


Thread & Core_thread::singleton()
{
	static Core_thread ct;
	return ct;
}
