/*
 * \brief  Kernel backend for execution contexts in userland
 * \author Martin Stein
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread_state.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <kernel/vm.h>
#include <platform_thread.h>
#include <platform_pd.h>

using namespace Kernel;

namespace Kernel
{
	typedef Genode::Thread_state Thread_state;
}

char const * Kernel::Thread::label()
{
	if (!platform_thread()) {
		if (!phys_utcb()) { return "idle"; }
		return "core";
	}
	return platform_thread()->name();
}


char const * Kernel::Thread::pd_label()
{
	if (core()) { return "core"; }
	if (!pd()) { return "?"; }
	return pd()->platform_pd()->label();
}


void
Kernel::Thread::prepare_to_start(void * const        ip,
                                 void * const        sp,
                                 unsigned const      cpu_id,
                                 unsigned const      pd_id,
                                 Native_utcb * const utcb_phys,
                                 Native_utcb * const utcb_virt,
                                 bool const          main)
{
	assert(_state == AWAITS_START)

	/* FIXME: support SMP */
	if (cpu_id) { PERR("multicore processing not supported"); }

	/* store thread parameters */
	_phys_utcb = utcb_phys;
	_virt_utcb = utcb_virt;
	_pd_id     = pd_id;

	/* join a protection domain */
	Pd * const pd = Pd::pool()->object(_pd_id);
	assert(pd);
	addr_t const tlb = pd->tlb()->base();

	/* initialize CPU context */
	User_context * const c = static_cast<User_context *>(this);
	bool const core = (_pd_id == core_id());
	if (!main) { c->init_thread(ip, sp, tlb, pd_id); }
	else if (!core) { c->init_main_thread(ip, utcb_virt, tlb, pd_id); }
	else { c->init_core_main_thread(ip, sp, tlb, pd_id); }

	/* print log message */
	if (START_VERBOSE) {
		PINF("in program %u '%s' start thread %u '%s'",
		      this->pd_id(), pd_label(), id(), label());
	}
}


Kernel::Thread::Thread(Platform_thread * const platform_thread)
:
	_platform_thread(platform_thread), _state(AWAITS_START),
	_pager(0), _pd_id(0), _phys_utcb(0), _virt_utcb(0),
	_signal_receiver(0)
{
	if (_platform_thread) { priority = _platform_thread->priority(); }
	else { priority = Kernel::Priority::MAX; }
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_pd()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to create protection domain");
		user_arg_0(0);
		return;
	}
	/* create translation lookaside buffer and protection domain */
	void * p = (void *)user_arg_1();
	Tlb * const tlb = new (p) Tlb();
	p = (void *)((addr_t)p + sizeof(Tlb));
	Pd * const pd = new (p) Pd(tlb, (Platform_pd *)user_arg_2());
	user_arg_0(pd->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_pd()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to destruct protection domain");
		user_arg_0(-1);
		return;
	}
	/* lookup protection domain */
	unsigned id = user_arg_1();
	Pd * const pd = Pd::pool()->object(id);
	if (!pd) {
		PERR("unknown protection domain");
		user_arg_0(-1);
		return;
	}
	/* destruct translation lookaside buffer and protection domain */
	Tlb * const tlb = pd->tlb();
	pd->~Pd();
	tlb->~Tlb();

	/* clean up buffers of memory management */
	Cpu::flush_tlb_by_pid(pd->id());
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_thread()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* dispatch arguments */
	Syscall_arg const arg1 = user_arg_1();
	Syscall_arg const arg2 = user_arg_2();

	/* create thread */
	Thread * const t = new ((void *)arg1)
	Thread((Platform_thread *)arg2);

	/* return thread ID */
	user_arg_0((Syscall_ret)t->id());
}

/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_delete_thread()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* get targeted thread */
	unsigned thread_id = (unsigned)user_arg_1();
	Thread * const thread = Thread::pool()->object(thread_id);
	assert(thread);

	/* destroy thread */
	thread->~Thread();
}

/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_start_thread()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* dispatch arguments */
	Platform_thread * pt = (Platform_thread *)user_arg_1();
	void * const ip = (void *)user_arg_2();
	void * const sp = (void *)user_arg_3();
	unsigned const cpu_id = (unsigned)user_arg_4();

	/* get targeted thread */
	Thread * const t = Thread::pool()->object(pt->id());
	assert(t);

	/* start thread */
	unsigned const pd_id = pt->pd_id();
	Native_utcb * const utcb_p = pt->phys_utcb();
	Native_utcb * const utcb_v = pt->virt_utcb();
	t->start(ip, sp, cpu_id, pd_id, utcb_p, utcb_v, pt->main_thread());

	/* return software TLB that the thread is assigned to */
	Pd::Pool * const pp = Pd::pool();
	Pd * const pd = pp->object(t->pd_id());
	assert(pd);
	user_arg_0((Syscall_ret)pd->tlb());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_pause_thread()
{
	unsigned const tid = user_arg_1();

	/* shortcut for a thread to pause itself */
	if (!tid) {
		pause();
		user_arg_0(0);
		return;
	}

	/* get targeted thread and check permissions */
	Thread * const t = Thread::pool()->object(tid);
	assert(t && (pd_id() == core_id() || this == t));

	/* pause targeted thread */
	t->pause();
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_resume_thread()
{
	/* lookup thread */
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}
	/* check permissions */
	if (pd_id() != core_id() && pd_id() != t->pd_id()) {
		PERR("not entitled to resume thread");
		user_arg_0(-1);
		return;
	}
	/* resume targeted thread */
	user_arg_0(t->resume());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_resume_faulter()
{
	/* lookup thread */
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) {
		PERR("unknown thread");
		user_arg_0(-1);
		return;
	}
	/* check permissions */
	if (pd_id() != core_id() && pd_id() != t->pd_id()) {
		PERR("not entitled to resume thread");
		user_arg_0(-1);
		return;
	}
	/* writeback translation table and resume faulter */
	Cpu::tlb_insertions();
	t->resume();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_yield_thread()
{
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (t) { t->receive_yielded_cpu(); }
	cpu_scheduler()->yield();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_current_thread_id()
{ user_arg_0((Syscall_ret)id()); }


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_get_thread()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to read address of platform thread");
		user_arg_0(0);
		return;
	}
	/* lookup thread */
	unsigned const id = user_arg_1();
	Thread * t;
	if (id) {
		t = Thread::pool()->object(id);
		if (!t) {
			PERR("unknown thread");
			user_arg_0(0);
		}
	} else { t = this; }
	user_arg_0((Syscall_ret)t->platform_thread());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_wait_for_request()
{
	wait_for_request();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_request_and_wait()
{
	/* get IPC receiver */
	Thread * const t = Thread::pool()->object(user_arg_1());
	assert(t);

	/* do IPC */
	request_and_wait(t, (size_t)user_arg_2());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_reply() {
	reply((size_t)user_arg_1(), (bool)user_arg_2()); }


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_set_pager()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to set pager");
		return;
	}
	/* lookup faulter and pager thread */
	unsigned const pager_id = user_arg_1();
	Thread * const pager    = Thread::pool()->object(pager_id);
	Thread * const faulter  = Thread::pool()->object(user_arg_2());
	if ((pager_id && !pager) || !faulter) {
		PERR("failed to set pager");
		return;
	}
	/* assign pager */
	faulter->pager(pager);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_update_pd()
{
	assert(pd_id() == core_id());
	Cpu::flush_tlb_by_pid(user_arg_1());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_update_region()
{
	assert(pd_id() == core_id());

	/* FIXME we don't handle instruction caches by now */
	Cpu::flush_data_cache_by_virt_region((addr_t)user_arg_1(),
										 (size_t)user_arg_2());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_allocate_irq()
{
	assert(pd_id() == core_id());
	unsigned irq = user_arg_1();
	user_arg_0(allocate_irq(irq));
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_free_irq()
{
	assert(pd_id() == core_id());
	unsigned irq = user_arg_1();
	user_arg_0(free_irq(irq));
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_await_irq()
{
	assert(pd_id() == core_id());
	await_irq();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_print_char()
{
	Genode::printf("%c", (char)user_arg_1());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_read_thread_state()
{
	assert(pd_id() == core_id());
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) PDBG("Targeted thread unknown");
	Thread_state * const ts = (Thread_state *)phys_utcb()->base();
	t->Cpu::Context::read_cpu_state(ts);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_write_thread_state()
{
	assert(pd_id() == core_id());
	Thread * const t = Thread::pool()->object(user_arg_1());
	if (!t) PDBG("Targeted thread unknown");
	Thread_state * const ts = (Thread_state *)phys_utcb()->base();
	t->Cpu::Context::write_cpu_state(ts);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_signal_receiver()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to create signal receiver");
		user_arg_0(0);
		return;
	}
	/* create receiver */
	void * p = (void *)user_arg_1();
	Signal_receiver * const r = new (p) Signal_receiver();
	user_arg_0(r->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_signal_context()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to create signal context");
		user_arg_0(0);
		return;
	}
	/* lookup receiver */
	unsigned id = user_arg_2();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* create and assign context*/
	void * p = (void *)user_arg_1();
	unsigned imprint = user_arg_3();
	if (r->new_context(p, imprint)) {
		PERR("failed to create signal context");
		user_arg_0(0);
		return;
	}
	/* return context name */
	Signal_context * const c = (Signal_context *)p;
	user_arg_0(c->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_await_signal()
{
	/* lookup receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(-1);
		return;
	}
	/* register handler at the receiver */
	if (r->add_handler(this)) {
		PERR("failed to register handler at signal receiver");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_signal_pending()
{
	/* lookup signal receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* get pending state */
	user_arg_0(r->deliverable());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_submit_signal()
{
	/* lookup signal context */
	unsigned const id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if(!c) {
		PERR("unknown signal context");
		user_arg_0(-1);
		return;
	}
	/* trigger signal context */
	if (c->submit(user_arg_2())) {
		PERR("failed to submit signal context");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_ack_signal()
{
	/* lookup signal context */
	unsigned id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PERR("unknown signal context");
		return;
	}
	/* acknowledge */
	c->ack();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_signal_context()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to kill signal context");
		user_arg_0(-1);
		return;
	}
	/* lookup signal context */
	unsigned id = user_arg_1();
	Signal_context * const c = Signal_context::pool()->object(id);
	if (!c) {
		PERR("unknown signal context");
		user_arg_0(0);
		return;
	}
	/* kill signal context */
	if (c->kill(this)) {
		PERR("failed to kill signal context");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_kill_signal_receiver()
{
	/* check permissions */
	if (pd_id() != core_id()) {
		PERR("not entitled to kill signal receiver");
		user_arg_0(-1);
		return;
	}
	/* lookup signal receiver */
	unsigned id = user_arg_1();
	Signal_receiver * const r = Signal_receiver::pool()->object(id);
	if (!r) {
		PERR("unknown signal receiver");
		user_arg_0(0);
		return;
	}
	/* kill signal receiver */
	if (r->kill(this)) {
		PERR("unknown signal receiver");
		user_arg_0(-1);
		return;
	}
	user_arg_0(0);
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_new_vm()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* dispatch arguments */
	void * const allocator = (void * const)user_arg_1();
	Genode::Cpu_state_modes * const state =
		(Genode::Cpu_state_modes * const)user_arg_2();
	Signal_context * const context =
		Signal_context::pool()->object(user_arg_3());
	assert(context);

	/* create vm */
	Vm * const vm = new (allocator) Vm(state, context);

	/* return vm id */
	user_arg_0((Syscall_ret)vm->id());
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_run_vm()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* run targeted vm */
	vm->run();
}


/**
 * Do specific syscall for this thread, for details see 'syscall.h'
 */
void Thread::_syscall_pause_vm()
{
	/* check permissions */
	assert(pd_id() == core_id());

	/* get targeted vm via its id */
	Vm * const vm = Vm::pool()->object(user_arg_1());
	assert(vm);

	/* pause targeted vm */
	vm->pause();
}


/**
 * Handle a syscall request
 */
void Thread::_syscall()
{
	switch (user_arg_0())
	{
	case NEW_THREAD :           _syscall_new_thread(); return;
	case DELETE_THREAD :        _syscall_delete_thread(); return;
	case START_THREAD :         _syscall_start_thread(); return;
	case PAUSE_THREAD :         _syscall_pause_thread(); return;
	case RESUME_THREAD :        _syscall_resume_thread(); return;
	case RESUME_FAULTER :       _syscall_resume_faulter(); return;
	case GET_THREAD :           _syscall_get_thread(); return;
	case CURRENT_THREAD_ID :    _syscall_current_thread_id(); return;
	case YIELD_THREAD :         _syscall_yield_thread(); return;
	case READ_THREAD_STATE :    _syscall_read_thread_state(); return;
	case WRITE_THREAD_STATE :   _syscall_write_thread_state(); return;
	case REQUEST_AND_WAIT :     _syscall_request_and_wait(); return;
	case REPLY :                _syscall_reply(); return;
	case WAIT_FOR_REQUEST :     _syscall_wait_for_request(); return;
	case SET_PAGER :            _syscall_set_pager(); return;
	case UPDATE_PD :            _syscall_update_pd(); return;
	case UPDATE_REGION :        _syscall_update_region(); return;
	case NEW_PD :               _syscall_new_pd(); return;
	case ALLOCATE_IRQ :         _syscall_allocate_irq(); return;
	case AWAIT_IRQ :            _syscall_await_irq(); return;
	case FREE_IRQ :             _syscall_free_irq(); return;
	case PRINT_CHAR :           _syscall_print_char(); return;
	case NEW_SIGNAL_RECEIVER :  _syscall_new_signal_receiver(); return;
	case NEW_SIGNAL_CONTEXT :   _syscall_new_signal_context(); return;
	case KILL_SIGNAL_CONTEXT :  _syscall_kill_signal_context(); return;
	case KILL_SIGNAL_RECEIVER : _syscall_kill_signal_receiver(); return;
	case AWAIT_SIGNAL :         _syscall_await_signal(); return;
	case SUBMIT_SIGNAL :        _syscall_submit_signal(); return;
	case SIGNAL_PENDING :       _syscall_signal_pending(); return;
	case ACK_SIGNAL :           _syscall_ack_signal(); return;
	case NEW_VM :               _syscall_new_vm(); return;
	case RUN_VM :               _syscall_run_vm(); return;
	case PAUSE_VM :             _syscall_pause_vm(); return;
	case KILL_PD :              _syscall_kill_pd(); return;
	default:
		PERR("invalid syscall");
		stop();
		reset_lap_time();
	}
}
